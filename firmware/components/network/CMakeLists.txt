idf_component_register(
    SRCS "networkmanager.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi lwip
)

idf_build_set_property(COMPILE_OPTIONS "-include sdkconfig" APPEND)