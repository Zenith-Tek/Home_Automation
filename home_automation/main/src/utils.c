#include "utils.h"
#include <stdio.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "rc_gpio.h"
#include "esp_timer.h"
#include "supabase.h"
#include "nvs_flash.h"

char *TAG = "UTILS";
uint32_t tick = 0;

// Mapping index 1-9 to the actual Relay GPIOs
const int RELAY_MAP[] = {0, 13, 14, 4, 16, 17, 5, 18, 19, 21};

// Mapping for the switches based on your new schematic
const int SWITCH_MAP[] = {0, 36, 39, 34, 35, 32, 33, 25, 26}; 

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
    relay_states[relay_id - 1] = 1; // Update local state
    ESP_LOGI(TAG, "Relay %d (GPIO %d) turned ON", relay_id, gpio_pin);
    
    // Sync change to Supabase
    update_relay_state_in_supabase(relay_id, 1);
    save_relay_states_to_nvs(); // Save to NVS for persistence
}

void relay_off(int relay_id)
{
    if (relay_id < 1 || relay_id > 9) return;

    int gpio_pin = RELAY_MAP[relay_id];
    gpio_set_level(gpio_pin, 0);
    relay_states[relay_id - 1] = 0; // Update local state
    ESP_LOGI(TAG, "Relay %d (GPIO %d) turned OFF", relay_id, gpio_pin);
    
    // Sync change to Supabase
    update_relay_state_in_supabase(relay_id, 0);
    save_relay_states_to_nvs(); // Save to NVS for persistence
}

/**
 * @brief Logic for physical switches.
 * This is called by the GPIO task when a switch event occurs.
 */
void process_gpios(int triggered_io) {
    int relay_to_toggle = -1;

    // Find which relay index matches the triggered Switch GPIO
    // Index 0 is onboard, 1-8 are external
    if (triggered_io == 0) relay_to_toggle = 1; 
    else if (triggered_io == 36) relay_to_toggle = 2;
    else if (triggered_io == 39) relay_to_toggle = 3;
    else if (triggered_io == 34) relay_to_toggle = 4;
    else if (triggered_io == 35) relay_to_toggle = 5;
    else if (triggered_io == 32) relay_to_toggle = 6;
    else if (triggered_io == 33) relay_to_toggle = 7;
    else if (triggered_io == 25) relay_to_toggle = 8;
    else if (triggered_io == 26) relay_to_toggle = 9;

    if (relay_to_toggle != -1) {
        // Toggle Logic: If current state is 1, set to 0. If 0, set to 1.
        int current_state = relay_states[relay_to_toggle - 1];
        int next_state = !current_state; 

        ESP_LOGW(TAG, "Switch IO%d pressed. Toggling Relay %d to %d", triggered_io, relay_to_toggle, next_state);
        
        // Execute the change
        control_relay(relay_to_toggle, next_state);
    }
}

