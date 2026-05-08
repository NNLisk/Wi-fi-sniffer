#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include <string.h>

#include "sniffer.h"
#include "packet_def.h"
#include "networkmanager.h"
#include "sdkconfig.h"

static const char *TAG = "SNIFFER";

static NetworkManager *nm = NULL;
static volatile int current_channel = 1;
static TaskHandle_t channel_hop_handle = NULL;

static packet_log_t packet_log[CONFIG_MAX_LOG_ENTRIES];
static int log_index = 0;


void sniffer_init(NetworkManager *nm_ptr) {
    nm = nm_ptr;
}


// Packet received callback
static void wifi_sniffer_cb(void *buf, wifi_promiscuous_pkt_type_t type) {    
    const wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *)buf;
    if (!ppkt) return;
    if (log_index >= CONFIG_MAX_LOG_ENTRIES) return;

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
        vTaskDelay(pdMS_TO_TICKS(CONFIG_CHANNEL_HOP_INTERVAL)); //time per channel
    }
}


// Sniff mode:
// - configures wifi for promiscuous
// - sets callback and filter for receiving packets
// - creates the task to channelhop, with a handle
void sniff_mode(void) {
    ESP_LOGI(TAG, "SNIFF MODE for 30s");

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
void sniff_mode_stop(void) {
    esp_wifi_set_promiscuous(false);
    if (channel_hop_handle) {
        vTaskDelete(channel_hop_handle);
        channel_hop_handle = NULL;
    }
}


// Transmit mode
// - Sets up wifi for transmitting
// - sends raw bytes over networkmanager
void transmit_mode(void) {
    ESP_LOGI(TAG, "ATTEMPTING TRANSMIT");

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
    ESP_LOGI(TAG, "packet_log_t size: %d", sizeof(packet_log_t));
    int sent = network_manager_send(nm, packet_log, total_bytes);

    if (sent > 0) {
        ESP_LOGI(TAG, "Successfully sent %d bytes (%d packets)", sent, log_index);
    } else {
        ESP_LOGE(TAG, "Failed to send packet data");
    }
}



// Cleans up transmit mode
void transmit_mode_stop(void) {
    network_manager_disconnect(nm);

    log_index = 0;
    memset(packet_log, 0, sizeof(packet_log));

    ESP_LOGI(TAG, "Transmitted");
}
