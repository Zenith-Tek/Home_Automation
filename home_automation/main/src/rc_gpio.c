#include "rc_gpio.h"
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "utils.h"
#include "supabase.h"

static const char *TAG_GPIO = "RC_GPIO";
static QueueHandle_t gpio_evt_queue = NULL;

// Arrays to handle multiple GPIOs (Must match the order in utils.c)
static const int SWITCH_PINS[] = {22, 36, 39, 34, 35, 32, 33, 25, 26};
static const int RELAY_PINS[]  = {13, 14, 4, 16, 17, 5, 18, 19, 21};
#define NUM_SWITCHES 9

// Per-switch last trigger timestamp for debounce (microseconds)
static int64_t last_trigger_time[NUM_SWITCHES] = {0};
#define DEBOUNCE_US 500000  // 500ms minimum between triggers on same switch

// GPIO34-39 are input-only and have NO internal pull-up on ESP32
static bool is_input_only_pin(int gpio) {
    return (gpio >= 34 && gpio <= 39);
}

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
            // Identify which relay matches this switch index
            int relay_idx = -1;
            for (int i = 0; i < NUM_SWITCHES; i++) {
                if (io_num == SWITCH_PINS[i]) {
                    relay_idx = i;
                    break;
                }
            }
            if (relay_idx == -1) continue;

            // Timestamp-based debounce: reject triggers too close together
            int64_t now = esp_timer_get_time();
            if ((now - last_trigger_time[relay_idx]) < DEBOUNCE_US) {
                continue;
            }

            // Debounce delay then re-verify level
            vTaskDelay(pdMS_TO_TICKS(80));

            if (gpio_get_level(io_num) == 0) {
                // For input-only pins (no internal pull-up), a floating pin
                // can hold low indefinitely. Require the pin to return high
                // (release) within a timeout to confirm a real switch press.
                if (is_input_only_pin(io_num)) {
                    bool released = false;
                    for (int w = 0; w < 50; w++) {  // Wait up to 1 second
                        vTaskDelay(pdMS_TO_TICKS(20));
                        if (gpio_get_level(io_num) == 1) {
                            released = true;
                            break;
                        }
                    }
                    if (!released) {
                        // Pin stuck low -- floating, not a real press
                        continue;
                    }
                    // Pin went high (released), confirming a real press-release cycle
                }

                last_trigger_time[relay_idx] = esp_timer_get_time();

                int new_state = !relay_states[relay_idx];
                process_gpios((int)io_num);
                ESP_LOGI(TAG_GPIO, "Switch IO%d toggled Relay %d to %d", 
                         (int)io_num, relay_idx + 1, new_state);

                // Wait for button release to prevent bouncing (for non-input-only pins)
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
        .intr_type = GPIO_INTR_NEGEDGE,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };

    // Only configure pins that have internal pull-ups.
    // Input-only pins (GPIO 34,35,36,39) are skipped -- they have no internal
    // pull-up and will float without external hardware. Re-enable them when
    // the production PCB with external pull-ups is used.
    uint64_t mask = 0;
    int enabled_count = 0;
    for (int i = 0; i < NUM_SWITCHES; i++) {
        if (!is_input_only_pin(SWITCH_PINS[i])) {
            mask |= (1ULL << SWITCH_PINS[i]);
            enabled_count++;
        } else {
            ESP_LOGW(TAG_GPIO, "Skipping GPIO%d (input-only, no pull-up)", SWITCH_PINS[i]);
        }
    }

    io_conf.pin_bit_mask = mask;
    gpio_config(&io_conf);

    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    xTaskCreate(gpio_event_task, "gpio_event_task", 8192, NULL, 10, NULL);

    gpio_install_isr_service(0);
    for (int i = 0; i < NUM_SWITCHES; i++) {
        if (!is_input_only_pin(SWITCH_PINS[i])) {
            gpio_isr_handler_add(SWITCH_PINS[i], gpio_isr_handler, (void*) SWITCH_PINS[i]);
        }
    }
    ESP_LOGI(TAG_GPIO, "%d switches enabled (%d input-only pins skipped)",
             enabled_count, NUM_SWITCHES - enabled_count);
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