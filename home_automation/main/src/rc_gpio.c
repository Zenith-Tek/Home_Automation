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
static int relay_state = 0;  // 0 = OFF, 1 = ON
extern int64_t last_manual_override_time_us;  // Declare to access from utils.c
// ISR handler - push GPIO number to queue
static void IRAM_ATTR gpio_isr_handler(void* arg) {
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

// Task to handle GPIO interrupts from queue
static void gpio_event_task(void* arg) {
    uint32_t io_num;
    while (1) {
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            // Small debounce delay (optional)
            vTaskDelay(pdMS_TO_TICKS(50));

            int level = gpio_get_level(io_num);
            ESP_LOGI(TAG_GPIO, "GPIO[%" PRIu32 "] interrupt, level: %d", io_num, level);

            // Trigger only on falling edge (switch press if using pull-up)
            if (level == 0 && io_num == SWITCH_GPIO) {
                relay_state = !relay_state;
                gpio_set_level(RELAY_GPIO, relay_state);
                ESP_LOGI(TAG_GPIO, "Relay toggled to: %d", relay_state);
                last_manual_override_time_us = esp_timer_get_time();  // Update override timestamp
                update_control_state_from_esp32(relay_state);
            }
        }
    }
}

void setup_gpios(void) {

    set_relay_gpios_out();
    set_switch_gpios_in();

    ESP_LOGI(TAG_GPIO, "GPIOs initialized: Relay = GPIO%d, Switch = GPIO%d", RELAY_GPIO, SWITCH_GPIO);
}

void set_switch_gpios_in(void){
    // Configure Switch GPIO (input with interrupt)
    // <--------------------- SWITCH_GPIO_ONB_1 ---------------------> //
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << SWITCH_GPIO_ONB_1);
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);

    // Create queue for ISR events
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    // Start task to process GPIO events
    xTaskCreate(gpio_event_task, "gpio_event_task", 8192, NULL, 10, NULL);

    // Install ISR service and attach handler
    gpio_install_isr_service(0);
    gpio_isr_handler_add(SWITCH_GPIO_ONB_2, gpio_isr_handler, (void*) SWITCH_GPIO_ONB_1);

    // <--------------------- SWITCH_GPIO_ONB_2 ---------------------> //
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << SWITCH_GPIO_ONB_2);
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);

    // Create queue for ISR events
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    // Start task to process GPIO events
    xTaskCreate(gpio_event_task, "gpio_event_task", 8192, NULL, 10, NULL);

    // Install ISR service and attach handler
    gpio_install_isr_service(0);
    gpio_isr_handler_add(SWITCH_GPIO_ONB_2, gpio_isr_handler, (void*) SWITCH_GPIO_ONB_2);

    // <--------------------- SWITCH_GPIO_1 ---------------------> //
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << SWITCH_GPIO_1);
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);

    // Create queue for ISR events
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    // Start task to process GPIO events
    xTaskCreate(gpio_event_task, "gpio_event_task", 8192, NULL, 10, NULL);

    // Install ISR service and attach handler
    gpio_install_isr_service(0);
    gpio_isr_handler_add(SWITCH_GPIO_1, gpio_isr_handler, (void*) SWITCH_GPIO_1);

    // <--------------------- SWITCH_GPIO_2 ---------------------> //
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << SWITCH_GPIO_2);
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);

    // Create queue for ISR events
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    // Start task to process GPIO events
    xTaskCreate(gpio_event_task, "gpio_event_task", 8192, NULL, 10, NULL);

    // Install ISR service and attach handler
    gpio_install_isr_service(0);
    gpio_isr_handler_add(SWITCH_GPIO_2, gpio_isr_handler, (void*) SWITCH_GPIO_2);

    // <--------------------- SWITCH_GPIO_3 ---------------------> //
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << SWITCH_GPIO_3);
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);

    // Create queue for ISR events
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    // Start task to process GPIO events
    xTaskCreate(gpio_event_task, "gpio_event_task", 8192, NULL, 10, NULL);

    // Install ISR service and attach handler
    gpio_install_isr_service(0);
    gpio_isr_handler_add(SWITCH_GPIO_3, gpio_isr_handler, (void*) SWITCH_GPIO_3);

    // <--------------------- SWITCH_GPIO_4 ---------------------> //
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << SWITCH_GPIO_4);
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);

    // Create queue for ISR events
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    // Start task to process GPIO events
    xTaskCreate(gpio_event_task, "gpio_event_task", 8192, NULL, 10, NULL);

    // Install ISR service and attach handler
    gpio_install_isr_service(0);
    gpio_isr_handler_add(SWITCH_GPIO_4, gpio_isr_handler, (void*) SWITCH_GPIO_4);

    // <--------------------- SWITCH_GPIO_5 ---------------------> //
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << SWITCH_GPIO_5);
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);

    // Create queue for ISR events
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    // Start task to process GPIO events
    xTaskCreate(gpio_event_task, "gpio_event_task", 8192, NULL, 10, NULL);

    // Install ISR service and attach handler
    gpio_install_isr_service(0);
    gpio_isr_handler_add(SWITCH_GPIO_5, gpio_isr_handler, (void*) SWITCH_GPIO_5);

    // <--------------------- SWITCH_GPIO_6 ---------------------> //
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << SWITCH_GPIO_6);
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);

    // Create queue for ISR events
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    // Start task to process GPIO events
    xTaskCreate(gpio_event_task, "gpio_event_task", 8192, NULL, 10, NULL);

    // Install ISR service and attach handler
    gpio_install_isr_service(0);
    gpio_isr_handler_add(SWITCH_GPIO_6, gpio_isr_handler, (void*) SWITCH_GPIO_6);

    // <--------------------- SWITCH_GPIO_7 ---------------------> //
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << SWITCH_GPIO_7);
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);

    // Create queue for ISR events
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    // Start task to process GPIO events
    xTaskCreate(gpio_event_task, "gpio_event_task", 8192, NULL, 10, NULL);

    // Install ISR service and attach handler
    gpio_install_isr_service(0);
    gpio_isr_handler_add(SWITCH_GPIO_7, gpio_isr_handler, (void*) SWITCH_GPIO_7);

    // <--------------------- SWITCH_GPIO_8 ---------------------> //
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << SWITCH_GPIO_8);
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);

    // Create queue for ISR events
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    // Start task to process GPIO events
    xTaskCreate(gpio_event_task, "gpio_event_task", 8192, NULL, 10, NULL);

    // Install ISR service and attach handler
    gpio_install_isr_service(0);
    gpio_isr_handler_add(SWITCH_GPIO_8, gpio_isr_handler, (void*) SWITCH_GPIO_8);
}

void set_relay_gpios_out(void){
      // Configure Relay GPIOs (output)
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << RELAY_GPIO_ONB),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << RELAY_GPIO_1),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << RELAY_GPIO_2),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << RELAY_GPIO_3),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << RELAY_GPIO_4),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << RELAY_GPIO_5),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << RELAY_GPIO_6),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << RELAY_GPIO_7),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << RELAY_GPIO_8),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    gpio_config(&io_conf);

}