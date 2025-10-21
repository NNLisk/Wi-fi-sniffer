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


// -- conf / globals ---------------------------------

static const char *SNF = "SNIFF MODE";
static const char *TRNSMT = "TRANSMIT MODE";
static const char *MAIN = "INIT";

#define HOSTSSID "sniffnet"
#define HOSTPW "sniff1234"

#define SERVERIP "SERVERIP"
#define PORT 50000

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

    // addr 4 also exists

} wifi_ieee80211_hdr_t;

typedef struct {
    wifi_ieee80211_hdr_t hdr; // Header data, MAC addresses and 
    uint8_t payload[]; // variable-length frame body
} wifi_ieee80211_packet_t;

typedef struct {
    uint32_t timestamp;
    int8_t rssi;
    uint8_t mac[6];
    uint8_t channel;
} packet_log_t;

#define MAX_LOG_ENTRIES 500
packet_log_t packet_log[MAX_LOG_ENTRIES];
int log_index = 0;

// -- forward decls -------------
static const char *logToJson(void);
static void sniff_mode(void);
static void transmit_mode(void);

// -- Sniffer callback -----------------------------

static void wifi_sniffer_cb(void *buf, wifi_promiscuous_pkt_type_t type) {    
    const wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *)buf;
    if (!ppkt) return;
    if (log_index >= MAX_LOG_ENTRIES) return;

    const uint8_t *raw = ppkt-> payload;
    if (!raw) return;

    const wifi_ieee80211_packet_t *ipkt = (wifi_ieee80211_packet_t *)raw;
    
    packet_log[log_index].timestamp = esp_log_timestamp();
    packet_log[log_index].rssi = ppkt->rx_ctrl.rssi;
    memcpy(packet_log[log_index].mac, ipkt->hdr.addr2, 6);
    packet_log[log_index].channel = current_channel;
    log_index++;
}

// changes channel every 2000 ms
static void channel_hop_task(void *arg) {
    int channel = 1;
    
    while (1) {
        current_channel = channel;
        esp_err_t err = esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
        if (err == ESP_OK) {
            ESP_LOGI(SNF, "Set Channel %d", channel);
        }
        channel ++;
        if (channel > 13) channel = 1;
        vTaskDelay(pdMS_TO_TICKS(500)); //time per channel
    }
}

//saving the log entries from RAM to a JSON file
//could do csv

// need to add checking that buffer has space left

static const char *logToJson(void) {
    static char buffer[8192];
    char *p = buffer;
    p += sprintf(p, "[");

    for (int i = 0; i < log_index; i++) {
        packet_log_t *e = &packet_log[i];
        p+= sprintf(p, 
            "{\"time\":%lu,\"mac\":\"%02X:%02X:%02X:%02X:%02X:%02X\","
            "\"rssi\":%d,\"ch\":%d}%s",
            e->timestamp,
            e->mac[0], e->mac[1], e->mac[2], e->mac[3], e->mac[4], e->mac[5],
            e->rssi,
            e->channel,
            (i == log_index - 1) ? "": ",");
    }
    sprintf(p, "]");
    return buffer;
}

static void sniff_mode(void) {
    ESP_LOGI(SNF, "SNIFF MODE for 30s");

    esp_wifi_stop();
    esp_wifi_set_mode(WIFI_MODE_NULL);
    esp_wifi_start();

    esp_wifi_set_ps(WIFI_PS_NONE);

    // Accept all packets
    // for some reason it has to be set with .filter_mask
    esp_wifi_set_promiscuous_rx_cb(&wifi_sniffer_cb);
    wifi_promiscuous_filter_t filter = {.filter_mask = WIFI_PROMIS_FILTER_MASK_ALL};
    esp_wifi_set_promiscuous_filter(&filter);
    esp_wifi_set_promiscuous(true);

    xTaskCreate(channel_hop_task, "ch_hop", 4096, NULL, 5, &channel_hop_handle);

    vTaskDelay(pdMS_TO_TICKS(30000));

    //kill channel hop task after 30s
    esp_wifi_set_promiscuous(false);
    if (channel_hop_handle) {
        vTaskDelete(channel_hop_handle);
        channel_hop_handle = NULL;
    }
}

// To do, connect to a server and transmit the json
static void transmit_mode(void) {
    ESP_LOGI(TRNSMT, "TRANSMIT MODE for 10s");
    const char *json = logToJson();
    
    esp_wifi_stop();
    esp_wifi_set_mode(WIFI_MODE_STA);


    wifi_config_t wifi_config = {
    .sta = {
        .ssid = HOSTSSID,
        .password = HOSTPW,
        }
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
    esp_wifi_connect();

    vTaskDelay(pdMS_TO_TICKS(5000));

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TRNSMT, "Unable to create socket: errno %d", errno);
        return;
    }

    //ipv4 address
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(SERVERIP);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);


    int err = sendto(sock, json, strlen(json), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err < 0) {
        ESP_LOGE(TRNSMT, "Error occurred during sending: errno %d", errno);
    } else {
        ESP_LOGI(TRNSMT, "Sent %d bytes", err);
    }
    log_index = 0;
}

static void modeSwitcher(void *arg) {
    while (1) {
        sniff_mode();
        vTaskDelay(pdMS_TO_TICKS(30000));
        transmit_mode();
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
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

    
    //starts loop
    xTaskCreatePinnedToCore(modeSwitcher, "modeSwitcher", 12*1024, NULL, 5, NULL, 1);
}

// -----------------------------------------------

