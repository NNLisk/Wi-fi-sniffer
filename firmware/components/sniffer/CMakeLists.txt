idf_component_register(
    SRCS "sniffer.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi freertos network packet
)