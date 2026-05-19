set(SRCS
    "src/main.c"
    "src/provisioning.c"
    "src/rc_gpio.c"
    "src/supabase.c"
    "src/utils.c"
    "src/wifi.c"
)

idf_component_register(SRCS "${SRCS}"
                       INCLUDE_DIRS "inc")
                    
