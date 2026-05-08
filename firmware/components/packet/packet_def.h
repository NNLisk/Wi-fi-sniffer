#include <stdint.h>

#ifndef PACKET_DEF_H
#define PACKET_DEF_H


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


// packet padding bytes removed packet size 13bytes
typedef struct {
    uint32_t timestamp;
    uint16_t framecontrol;
    int8_t rssi;
    uint8_t mac[6];
    uint8_t channel;
} __attribute__((packed)) packet_log_t;



#endif