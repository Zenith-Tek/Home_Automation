#include "rc_gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "utils.h"
#include "supabase.h"

static const char *TAG_GPIO = "RC_GPIO";
static QueueHandle_t gpio_evt_queue = NULL;

// Arrays to handle multiple GPIOs (Must match the order in utils.c)
static const int SWITCH_PINS[] = {0, 36, 39, 34, 35, 32, 33, 25, 26};
static const int RELAY_PINS[]  = {13, 14, 4, 16, 17, 5, 18, 19, 21};

// ISR handler - pushes GPIO number to queue
static void IRAM_ATTR gpio_isr_handler(void* arg) {
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

// THE SINGLE TASK to handle all switch events
static void gpio_event_task(void* arg) {
    uint32_t io_num;
    while (1) {
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            // 1. Debounce delay
            vTaskDelay(pdMS_TO_TICKS(80));

            // 2. Double-check if still pressed (Logic 0 because of 10K pull-ups)
            if (gpio_get_level(io_num) == 0) {
                
                // Identify which relay matches this switch index
                int relay_idx = -1;
                for (int i = 0; i < 9; i++) {
                    if (io_num == SWITCH_PINS[i]) {
                        relay_idx = i;
                        break;
                    }
                }

                if (relay_idx != -1) {
                    // Toggle current state
                    int new_state = !relay_states[relay_idx];

                    // Execute change via utils logic
                    process_gpios((int)io_num);

                    // LOGGING: Fixed the format error here (%d for an integer)
                    ESP_LOGI(TAG_GPIO, "Switch IO%d toggled Relay %d to %d", 
                             (int)io_num, relay_idx + 1, new_state);
                }

                // 3. Wait for button release to prevent bouncing
                while(gpio_get_level(io_num) == 0) {
                    vTaskDelay(pdMS_TO_TICKS(20));
                }
            }
        }
    }
}

void setup_gpios(void) {
    set_relay_gpios_out();
    set_switch_gpios_in();
    ESP_LOGI(TAG_GPIO, "All 9 Relays and 9 Switches initialized.");
}

void set_switch_gpios_in(void) {
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_NEGEDGE, // Falling edge (Pressing the button)
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };

    uint64_t mask = 0;
    for (int i = 0; i < 9; i++) {
        mask |= (1ULL << SWITCH_PINS[i]);
    }
    io_conf.pin_bit_mask = mask;
    gpio_config(&io_conf);

    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    xTaskCreate(gpio_event_task, "gpio_event_task", 8192, NULL, 10, NULL);

    gpio_install_isr_service(0);
    for (int i = 0; i < 9; i++) {
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

    uint64_t mask = 0;
    for (int i = 0; i < 9; i++) {
        mask |= (1ULL << RELAY_PINS[i]);
    }
    io_conf.pin_bit_mask = mask;
    gpio_config(&io_conf);
}