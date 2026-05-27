set(SRCS
    "src/main.c"
    "src/provisioning.c"
    "src/rc_gpio.c"
    "src/supabase.c"
    "src/utils.c"
    "src/wifi.c"
)

file(GLOB_RECURSE SOURCES "*.c")

idf_component_register(SRCS ${SOURCES}
                    INCLUDE_DIRS "inc" "."
                    PRIV_REQUIRES 
                        esp_wifi 
                        esp_event 
                        esp_netif
                        nvs_flash 
                        wifi_provisioning
                        lwip
                        esp_http_client 
                        esp_websocket_client 
                        json 
                        driver
                        esp_timer
                        esp_system
                        esp_partition
                        esp_app_format
                        app_update
                        spi_flash
                        mbedtls
)
