set(SRCS
    "src/main.c"
    "src/provisioning.c"
    "src/rc_gpio.c"
    "src/supabase.c"
    "src/utils.c"
    "src/wifi.c"
)

idf_component_register(SRCS ${SRCS}
                    INCLUDE_DIRS "inc" "."
                    PRIV_REQUIRES 
                        esp_wifi 
                        esp_event 
                        esp_netif
                        nvs_flash 
                        wifi_provisioning
                        lwip
                        esp_http_client 
                        esp_https_ota        # Required for OTA
                        esp_websocket_client # Downloaded via Component Manager
                        json                 # For cJSON
                        driver               # For GPIOs
                        esp_timer 
                        esp_system 
                        esp_partition 
                        esp_app_format       # For reading version info
                        app_update           # For partition logic
                        spi_flash
                        mbedtls 
)
