#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#include "networkmanager.h"
#include "sniffer.h"        // sniff_mode, sniff_mode_stop, packet_log, log_index
#include "packet.h"         // packet_log_t

#include "sdkconfig.h"
// kconfig reference

// MAX_LOG_ENTRIES
// CHANNEL_HOP_INTERVAL
// SNIFF_TIME_PER_CYCLE
// TRANSMIT_TIME_PER_CYCLE

// SERVERIP
// PORT
// HOSTSSID
// HOSTPW


// fields

static const char *TAG = "MAIN";
static NetworkManager *nm = NULL;


// Mode switcher, since esp32 
// - n seconds listen
// - n seconds transmit
void mode_switcher(void *args) {
    while(1) {
        sniff_mode();
        vTaskDelay(pdMS_TO_TICKS(CONFIG_SNIFF_TIME_PER_CYCLE));
        sniff_mode_stop();

        transmit_mode();
        transmit_mode_stop();
    }
} 

// MAIN
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
    sniffer_init(nm);
    
    // mode switcher task
    xTaskCreatePinnedToCore(mode_switcher, "modeSwitcher", 12*1024, NULL, 5, NULL, 1);

}


