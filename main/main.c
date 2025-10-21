#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"

// -- Definitions ---------------------------

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

// --------------------------------------------

// tag for the log
static const char *TAG = "sniffer";

static void wifi_sniffer_cb(void *buf, wifi_promiscuous_pkt_type_t type) {
    ESP_LOGI(TAG, "callback");
    
    const wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *)buf;
    const wifi_ieee80211_packet_t *ipkt = (wifi_ieee80211_packet_t *)ppkt->payload;
    const uint8_t *frame = ipkt->payload;
    char mac[18];
    sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X",
            ipkt->hdr.addr2[0], ipkt->hdr.addr2[1],
            ipkt->hdr.addr2[2], ipkt->hdr.addr2[3],
            ipkt->hdr.addr2[4], ipkt->hdr.addr2[5]);

    uint16_t len = ppkt->rx_ctrl.sig_len;
    ESP_LOGI(TAG, "Pkt type=%d len=%d, srcmac=%s rssi=%d", type, len, mac, ppkt->rx_ctrl.rssi);
}

// changes channel every 2000 ms
static void channel_hop_task(void *arg) {
    int channel = 1;
    
    while (1) {
        esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
        ESP_LOGI(TAG, "Set Channel %d", channel);
        channel ++;
        if (channel > 13) channel = 1;
        vTaskDelay(pdMS_TO_TICKS(2000)); //time per channel
    }
}

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
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();
    esp_wifi_set_ps(WIFI_PS_NONE);
    esp_wifi_set_promiscuous_rx_cb(&wifi_sniffer_cb);

    // Accept all packets
    // for some reason it has to be set with .filter_mask
    wifi_promiscuous_filter_t filter = 
    {
        .filter_mask = WIFI_PROMIS_FILTER_MASK_ALL
    }; 
    esp_wifi_set_promiscuous_filter(&filter);
    esp_wifi_set_promiscuous(true);
    xTaskCreatePinnedToCore(channel_hop_task, "ch_hop", 4096, NULL, 5, NULL, 1);
}