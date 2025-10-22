# Wi-fi Sniffer

## DISCLAIMER
+ This project is purely for the purposes of learning, and demonstrating possible ways to analyze traffic
+ Project includes storing MAC addressess and other information to a database
+ It is not intended to be used to identify/de-anonymize internet users

## Project overview
ESP32 devices run in promiscuous/monitor mode, capture 802.11 frames, parse important fields to a JSON and send it to a Raspberry Pi over UDP. The Raspberry Pi updates records into a database, and it can be used later.

## Example infrastructure

```
 .------------.     .------------.     .------------.
 |  ESP32 #1  |     |  ESP32 #2  |     |  ESP32 #3  |
 '------+-----'     '------+-----'     '------+-----'
        |                  |                  |
        |          .---------------.          |
        |__________| Raspberry pi  |__________|
                   |     as an     |
                   | Access Point  |
                   '---------------'
```

## Hardware I use
- Espressif ESP32-WROOM-32D (ESP32 DevKitC)
- Raspberry Pi 3B (Acting as its own AP)
- (Optional) External antenna for increased transmit range
  
## Current goal
- Multiple ESP32 sniffers capture 802.11 management frames and send parsed JSON to a centralized UDP endpoint on a Raspberry Pi.
- Raspberry Pi parses incoming messages and writes entries to a database.
- Visualize data (heatmap, RSSI-over-time graphs, device density, etc.).

# Problems
## MAC randomization
Most mobile devices currently randomize their MAC addresses, and this makes it difficult to track any single device. Essentially a phone might send a probe request under one MAC address and another probe request under another MAC address. This results in a MAC address having usually one or few entries in the database --> It would look like the network has tons of devices even though there might only be a few.

Possible solutions:
collecting packet data from 802.11 frame control fields -- this would potentially make it possible to identify/group multiple similar packets as one device even though under a different MAC address. Frame control data in 802.11 contains for example IE data that can be 'fingerprinted' and might stay consistent over mac addresses

Simplified 802.11 packet with the fields we're most interested in
```
Bytes 0-2                       Bytes 10-16                  PAYLOAD
+---------------------+--------+---------------------+------+---------------------+-------+
| Frame Control       | ...    | ADDR2 (the source)  | ...  | Frame body          | FCS   |
+---------------------+--------+---------------------+------+---------------------+-------+
```
In addition we can reduce the amount of packets by deciding that we're not interested in beacon packets, since they are usually sent by e.g. routers and likely is most of the entries. A few studies show average mobile probe requests to be more bursty, but still much less frequent than router beacon packets

# Requirements
+ ESP-IDF
+ ESP32 microcontroller
+ UDP listener and database can be run off of anything that can act as an access point

