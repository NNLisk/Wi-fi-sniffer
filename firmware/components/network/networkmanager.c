#include "networkmanager.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include <string.h>

#include "sdkconfig.h"

#define HOSTSSID "sniffnet"
#define HOSTPW "sniff1234"

#define SERVERIP "192.168.100.12"
#define PORT 58585

static const char *TAG = "NET_MGR";

struct NetworkManager {
    char host[64];
    int port;
    int socket_fd;
    bool connected;
    struct sockaddr_in dest_addr;
};

NetworkManager* network_manager_create(void) {
    NetworkManager* nm = malloc(sizeof(NetworkManager));
    if (!nm) {
        ESP_LOGE(TAG, "Failed to create network manager");
        return NULL;
    }

    strncpy(nm -> host, SERVERIP, sizeof(nm->host) - 1);
    nm->port = PORT;
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

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = HOSTSSID,
            .password = HOSTPW
        },
    };

    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
    esp_wifi_connect();


    vTaskDelay(pdMS_TO_TICKS(5000));

    nm->socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (nm->socket_fd < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        return -1;
    }

    //ipv4 address
    struct sockaddr_in dest_addr;
    nm->dest_addr.sin_addr.s_addr = inet_addr(SERVERIP);
    nm->dest_addr.sin_family = AF_INET;
    nm->dest_addr.sin_port = htons(PORT);
   

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