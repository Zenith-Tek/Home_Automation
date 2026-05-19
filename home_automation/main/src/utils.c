#include "utils.h"
#include <stdio.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "rc_gpio.h"
#include "esp_timer.h"
#include "supabase.h"

char *TAG = "UTILS";
uint32_t tick = 0;

// Mapping index 1-9 to the actual GPIOs from your schematic image
const int RELAY_MAP[] = {0, 13, 14, 4, 16, 17, 5, 18, 19, 21};

void control_relay(int relay_id, int state)
{
    if(state) {
        relay_on(relay_id);
    } else {   
        relay_off(relay_id);
    }
}

void relay_on(int relay_id)
{ 
    if (relay_id < 1 || relay_id > 9) return;

    int gpio_pin = RELAY_MAP[relay_id];
    gpio_set_level(gpio_pin, 1);
    ESP_LOGI(TAG, "Relay %d (GPIO %d) turned ON", relay_id, gpio_pin);
    
    // Sync change to Supabase
    update_relay_state_in_supabase(relay_id, 1);
}

void relay_off(int relay_id)
{
    if (relay_id < 1 || relay_id > 9) return;

    int gpio_pin = RELAY_MAP[relay_id];
    gpio_set_level(gpio_pin, 0);
    ESP_LOGI(TAG, "Relay %d (GPIO %d) turned OFF", relay_id, gpio_pin);
    
    // Sync change to Supabase
    update_relay_state_in_supabase(relay_id, 0);
}

void process_gpios() {
    // Logic for physical switches goes here
}