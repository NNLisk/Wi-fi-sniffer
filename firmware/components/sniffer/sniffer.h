#include "networkmanager.h"
#include "esp_wifi.h"


#ifndef SNIFFER_H
#define SNIFFER_H

void sniffer_init(NetworkManager *nm_ptr);
static void wifi_sniffer_cb(void *buf, wifi_promiscuous_pkt_type_t type);
static void channel_hop_task(void *arg);
void sniff_mode(void);
void sniff_mode_stop(void);
void transmit_mode(void);
void transmit_mode_stop(void);

#endif