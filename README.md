# Wi-fi Sniffer

# specs
+ Espressif ESP32-Wroom-32D module on ESP32 devkitc
+ Project listens to 802.11 packets in promiscuous mode,
  so without a filter. Should listen on all channels
+ Uses freeRTOS to schedule tasks

# Plan
+ to implement a system where ESP32 boards in different physical locations transmit relevant packet information to the e.g. central raspberry pi listening to a TCPsocket.

+ options for visualization
    + [----------------] intensity meters
      Display an incoming datapacket based on its intensity (dbm) as a flashing of one of the dashes, real time is problematic with a bulk json only every 30 seconds
    + visual mapping of physical locations of devices, updates every 30 seconds. more complicated, requires triangulation to compute the physical location. Also difficult with randomized MACs

# Problems
+ ESP32 cannot at the same time listen in promiscuous mode and 
  send the data somewhere.
  + currently I solve it by switching between 'Listen mode' and
    'broadcast mode'
+ Identifying devices, as most handhelds and laptops randomize their MAC addresses
  + Could identify multiple MAC addresses with consistent location as one device
  + prone to errors with dense areas

## Folder structure
```
├── CMakeLists.txt
├── main
│   ├── CMakeLists.txt
│   └── main.c
└── README.md     
```     

