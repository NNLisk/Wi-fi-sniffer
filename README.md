# Wi-fi Sniffer

## specs
+ Espressif ESP32-Wroom-32D module
+ Project listens to 802.11 packets in promiscuous mode,
  so without a filter. Should listen on all channels
+ (tentatively) connects to a defined hotspot and sends data to the listener
+ Uses freeRTOS to schedule tasks
+ Kconfig for menu configuration


## Usage
```
# connect your esp device with a usb

idf.py menuconfig

# navigate to component config -> sniffer networking configuration
# change the ssid, password to your match your hotspot
# change the ip address to match the host devices ip (typically something like "local area connection <n>" in  ipconfig)

idf.py build flash
```

then run the python listener on your device with the hotspot on

## requires

+ esp-idf
+ python

## topology

```
[ sniffer ] -------> [ central device ACCESSPOINT ] <---------- [ sniffer ]

# the sniffers connect to the accesspoint
# with three sniffers the plan is to triangulate packets
# and be able to map the traffic of an area e.g. with a heatmap
# physical location can group the mac addresses although identification
# isn't the purpose of this project
```


## Problems
+ ESP32 cannot at the same time listen in promiscuous mode and 
  send the data somewhere.
  + currently I solve it by switching between 'Listen mode' and
    'broadcast mode'
+ Identifying devices, as most handhelds and laptops randomize their MAC addresses
  + Could identify multiple MAC addresses with consistent location as one device
  + prone to errors with dense areas



