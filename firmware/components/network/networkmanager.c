#include "networkmanager.h"
#include "esp_wifi.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include <string.h>

#include "sdkconfig.h"

#define WIFI_CONNECTED_BIT BIT0

static EventGroupHandle_t wifi_events;
static const char *TAG = "NET_MGR";


// wifi_config_t ap_config = {
//     .ap = {
//         .ssid = AP_SSID,
//         .password = AP_PASSWORD,
//         .max_connection = 1,
//         .authmode = WIFI_AUTH_WPA2_PSK,
//     },
// };

wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_HOSTSSID,
            .password = CONFIG_HOSTPW
        },
    };

struct NetworkManager {
    char host[64];
    int port;
    int socket_fd;
    bool connected;
    struct sockaddr_in dest_addr;
};

static void wifi_event_handler(void *arg, esp_event_base_t base,
                                int32_t id, void *data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "WiFi disconnected");
        xEventGroupClearBits(wifi_events, WIFI_CONNECTED_BIT);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "Got IP");
        xEventGroupSetBits(wifi_events, WIFI_CONNECTED_BIT);
    }
}

NetworkManager* network_manager_create(void) {
    NetworkManager* nm = malloc(sizeof(NetworkManager));
    if (!nm) {
        ESP_LOGE(TAG, "Failed to create network manager");
        return NULL;
    }

    esp_netif_create_default_wifi_sta();
    wifi_events = xEventGroupCreate();
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);


    strncpy(nm -> host, CONFIG_SERVERIP, sizeof(nm->host) - 1);
    nm->port = CONFIG_PORT;
    nm->socket_fd = -1;
    nm->connected = false;
    return nm;
}

void network_manager_destroy(NetworkManager* nm) {
    if (!nm) return;
    
    if (nm->socket_fd >= 0) {
        close(nm->socket_fd);
    }
    
    free(nm);
}



int network_manager_connect(NetworkManager* nm) {
    esp_wifi_stop();
    esp_wifi_set_mode(WIFI_MODE_STA);
    
    // esp_wifi_set_mode(WIFI_MODE_AP);
    // esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    // esp_wifi_start();


    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
    esp_wifi_connect();


    EventBits_t bits = xEventGroupWaitBits(wifi_events, WIFI_CONNECTED_BIT,
                                        pdFALSE, pdTRUE, pdMS_TO_TICKS(10000));
    if (!(bits & WIFI_CONNECTED_BIT)) {
        ESP_LOGE(TAG, "WiFi connect timeout");
        return -1;
    }

    nm->socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (nm->socket_fd < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        return -1;
    }

    //ipv4 address
    struct sockaddr_in dest_addr;
    nm->dest_addr.sin_addr.s_addr = inet_addr(CONFIG_SERVERIP);
    nm->dest_addr.sin_family = AF_INET;
    nm->dest_addr.sin_port = htons(CONFIG_PORT);
   

    ESP_LOGI(TAG, "Connecting");
    int err = connect(nm->socket_fd, (struct sockaddr *)&nm->dest_addr, sizeof(nm->dest_addr));

    if (err != 0) {
        ESP_LOGE(TAG, "TCP connect failed: errno %d", errno);
        close(nm->socket_fd);
        nm->socket_fd = -1;
        return -1;
    }

    nm->connected = true;
    ESP_LOGI(TAG, "Connection successful");
    return 0;
}

int network_manager_disconnect(NetworkManager* nm) {
    if (!nm) return -1;

    if (nm->socket_fd >= 0) {
        close(nm->socket_fd);
        nm->socket_fd = -1;
    }

    esp_wifi_disconnect();
    nm->connected = false;

    return 0;
}

int network_manager_send(NetworkManager* nm, const void* data, size_t len) {
    if (!nm || !data) return -1;
    if (nm->socket_fd <  0) return -1;

    int err = send(nm->socket_fd, data, len, 0);

    if (err < 0) {
        ESP_LOGE(TAG, "Send failed: errno %d", errno);
        return -1;
    }

    ESP_LOGI(TAG, "Sent %d bytes via TCP", err);
    return err;
}