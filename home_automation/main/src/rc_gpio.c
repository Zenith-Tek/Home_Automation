#include "rc_gpio.h"
#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "utils.h"
#include "supabase.h"

static const char *TAG_GPIO = "RC_GPIO";
static QueueHandle_t gpio_evt_queue = NULL;

// Arrays to handle multiple GPIOs easily
static const int RELAY_PINS[] = {
    RELAY_GPIO_ONB, RELAY_GPIO_1, RELAY_GPIO_2, RELAY_GPIO_3, 
    RELAY_GPIO_4, RELAY_GPIO_5, RELAY_GPIO_6, RELAY_GPIO_7, RELAY_GPIO_8
};

static const int SWITCH_PINS[] = {
    SWITCH_GPIO_ONB_1, SWITCH_GPIO_1, SWITCH_GPIO_2, SWITCH_GPIO_3, 
    SWITCH_GPIO_4, SWITCH_GPIO_5, SWITCH_GPIO_6, SWITCH_GPIO_7, SWITCH_GPIO_8
};

#define NUM_RELAYS (sizeof(RELAY_PINS) / sizeof(RELAY_PINS[0]))

// ISR handler - push GPIO number to queue
static void IRAM_ATTR gpio_isr_handler(void* arg) {
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

// Single task to handle all switch events
static void gpio_event_task(void* arg) {
    uint32_t io_num;
    while (1) {
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            // Debounce delay
            vTaskDelay(pdMS_TO_TICKS(50));

            // Identify which relay matches this switch
            int relay_index = -1;
            for (int i = 0; i < NUM_RELAYS; i++) {
                if (io_num == SWITCH_PINS[i]) {
                    relay_index = i;
                    break;
                }
            }

            if (relay_index != -1) {
                // Toggle Logic
                int current_level = gpio_get_level(RELAY_PINS[relay_index]);
                int new_state = !current_level;

                // 1. Update Hardware
                gpio_set_level(RELAY_PINS[relay_index], new_state);
                
                // 2. Update local state array (defined in supabase.c)
                relay_states[relay_index] = new_state;

                // 3. Update Supabase (Relay ID in DB starts at 1, so we use index + 1)
                update_relay_state_in_supabase(relay_index + 1, new_state);

                ESP_LOGI(TAG_GPIO, "Switch GPIO %d toggled Relay %d (GPIO %d) to %d", 
                         (int)io_num, relay_index + 1, RELAY_PINS[relay_index], new_state);
            }
        }
    }
}

void setup_gpios(void) {

    set_relay_gpios_out();
    set_switch_gpios_in();

    ESP_LOGI(TAG_GPIO, "Successfully initialized %d Relays and %d Switches.", (int)NUM_RELAYS, (int)NUM_RELAYS);
}

void set_switch_gpios_in(void) {
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_NEGEDGE, // Trigger on press
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };

    // Construct a bitmask of all switch pins
    uint64_t pin_mask = 0;
    for (int i = 0; i < NUM_RELAYS; i++) {
        pin_mask |= (1ULL << SWITCH_PINS[i]);
    }

    io_conf.pin_bit_mask = pin_mask;
    gpio_config(&io_conf);

    // Create queue once
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    xTaskCreate(gpio_event_task, "gpio_event_task", 4096, NULL, 10, NULL);

    // Install ISR service once
    gpio_install_isr_service(0);

    // Attach handler for each switch
    for (int i = 0; i < NUM_RELAYS; i++) {
        gpio_isr_handler_add(SWITCH_PINS[i], gpio_isr_handler, (void*) SWITCH_PINS[i]);
    }
}

void set_relay_gpios_out(void) {
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };

    // Construct a bitmask of all relay pins
    uint64_t pin_mask = 0;
    for (int i = 0; i < NUM_RELAYS; i++) {
        pin_mask |= (1ULL << RELAY_PINS[i]);
    }
    
    io_conf.pin_bit_mask = pin_mask;
    gpio_config(&io_conf);
}
