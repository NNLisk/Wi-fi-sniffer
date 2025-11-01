#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"

#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"

#include "networkmanager.h"
#include "sdkconfig.h"

// -- Kconfig reference --------------------

// MAX_LOG_ENTRIES
// CHANNEL_HOP_INTERVAL
// SNIFF_TIME_PER_CYCLE
// TRANSMIT_TIME_PER_CYCLE


// -- Fields ---------------------------------

static const char *TAG = "MAIN";
static NetworkManager *nm = NULL;

static int current_channel = 1;
static TaskHandle_t channel_hop_handle = NULL;

// -- Structs ---------------------------

typedef struct {
    // first 2 bytes of the frame
    // contains e.g. 
    // protocol version, 
    // frame type,
    // retransmission flags
    // useful for filtering out the data packets later
    
    uint16_t frame_ctrl; 
    uint16_t duration_id;
    uint8_t addr1[6]; // Destination MAC
    uint8_t addr2[6]; // Source MAC
    uint8_t addr3[6]; // BSSID
    uint16_t seq_ctrl; // sequence info, not important here

} wifi_ieee80211_hdr_t;

typedef struct {
    wifi_ieee80211_hdr_t hdr; // Header data, MAC addresses and 
    uint8_t payload[]; // variable-length frame body
} wifi_ieee80211_packet_t;

typedef struct {
    uint32_t timestamp;
    uint8_t framecontrol;
    int8_t rssi;
    uint8_t mac[6];
    uint8_t channel;
} packet_log_t;


packet_log_t packet_log[MAX_LOG_ENTRIES];
int log_index = 0;


// Packet received callback
static void wifi_sniffer_cb(void *buf, wifi_promiscuous_pkt_type_t type) {    
    const wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *)buf;
    if (!ppkt) return;
    if (log_index >= MAX_LOG_ENTRIES) return;

    const uint8_t *raw = ppkt-> payload;
    if (!raw) return;

    const wifi_ieee80211_packet_t *ipkt = (wifi_ieee80211_packet_t *)raw;
    
    packet_log[log_index].framecontrol = ipkt->hdr.frame_ctrl;
    packet_log[log_index].timestamp = esp_log_timestamp();
    packet_log[log_index].rssi = ppkt->rx_ctrl.rssi;
    memcpy(packet_log[log_index].mac, ipkt->hdr.addr2, 6);
    packet_log[log_index].channel = current_channel;
    log_index++;
}

// changes channel every n ms
static void channel_hop_task(void *arg) {
    int channel = 1;
    
    while (1) {
        current_channel = channel;
        esp_err_t err = esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Set Channel %d", channel);
        }
        channel ++;
        if (channel > 13) channel = 1;
        vTaskDelay(pdMS_TO_TICKS(CHANNEL_HOP_INTERVAL)); //time per channel
    }
}



// Sniff mode:
// - configures wifi for promiscuous
// - sets callback and filter for receiving packets
// - creates the task to channelhop, with a handle
static void sniff_mode(void) {
    ESP_LOGI(TAG, "SNIFF MODE for 30s");

    if (network_manager_connect(nm) == 0) {
        ESP_LOGI(TAG, "Disconnecting from host");
        network_manager_disconnect(nm);
    } 

    esp_wifi_stop();
    esp_wifi_set_mode(WIFI_MODE_NULL);
    esp_wifi_start();

    esp_wifi_set_ps(WIFI_PS_NONE);

   
    esp_wifi_set_promiscuous_rx_cb(&wifi_sniffer_cb);
    wifi_promiscuous_filter_t filter = {.filter_mask = WIFI_PROMIS_FILTER_MASK_ALL};
    esp_wifi_set_promiscuous_filter(&filter);
    esp_wifi_set_promiscuous(true);

    xTaskCreate(channel_hop_task, "ch_hop", 4096, NULL, 5, &channel_hop_handle);
}


//Cleans up sniff mode
static void sniff_mode_stop(void) {
    esp_wifi_set_promiscuous(false);
    if (channel_hop_handle) {
        vTaskDelete(channel_hop_handle);
        channel_hop_handle = NULL;
    }
}



// Transmit mode
// - Sets up wifi for transmitting
// - sends raw bytes over networkmanager
static void transmit_mode(void) {
    ESP_LOGI(TAG, "TRANSMIT MODE for 10s");

    if (log_index == 0) {
        ESP_LOGI(TAG, "No packets to send");
    }

    
    if (!nm) {
        ESP_LOGE(TAG, "Network manager cannot be created");
        return;
    }

    if (network_manager_connect(nm) != 0) {
        ESP_LOGE(TAG, "Connection Failed");
        return;
    }

    int count = log_index;

    if (network_manager_send(nm, &count, sizeof(count)) < 0) {
        ESP_LOGE(TAG, "Failed to send packet count");
        network_manager_disconnect(nm);
        return;
    }

    size_t total_bytes = log_index * sizeof(packet_log_t);
    int sent = network_manager_send(nm, packet_log, total_bytes);

    if (sent > 0) {
        ESP_LOGI(TAG, "Successfully sent %d bytes (%d packets)", sent, log_index);
    } else {
        ESP_LOGE(TAG, "Failed to send packet data");
    }
}



// Cleans up transmit mode
static void transmit_mode_stop(void) {
    network_manager_disconnect(nm);

    log_index = 0;
    memset(packet_log, 0, sizeof(packet_log));

    ESP_LOGI(TAG, "Transmitted");
}


// Mode switcher, since esp32 
// - n seconds listen
// - n seconds transmit
void mode_switcher(void *args) {
    sniff_mode();
    vTaskDelay(pdMS_TO_TICKS(SNIFF_TIME_PER_CYCLE));
    sniff_mode_stop();

    transmit_mode();
    vTaskDelay(pdMS_TO_TICKS(TRANSMIT_TIME_PER_CYCLE));
    transmit_mode_stop();
} 

// -- MAIN ----------------------------------------
void app_main(void)
{
    /* Initializes the non volatile storage*/
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // initiate and configure network interface and wifi module
    esp_netif_init();
    esp_event_loop_create_default();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);

    nm = network_manager_create();
    
    // mode switcher task
    xTaskCreatePinnedToCore(mode_switcher, "modeSwitcher", 12*1024, NULL, 5, NULL, 1);

    network_manager_destroy(nm);
}
// -----------------------------------------------

