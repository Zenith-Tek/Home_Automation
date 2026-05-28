#ifndef __MAIN_H__
#define __MAIN_H__

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "spi_flash_mmap.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_wifi.h"
#include "esp_sntp.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_app_format.h"
#include "nvs_flash.h"

#include "utils.h"
#include "rc_gpio.h"

void print_system_memory_status();
void initialize_sntp(void);
void update_device_status(void);
void heartbeat_task(void *pvParameters);
void handle_provisioning_timestamp();
const char* get_reboot_reason_string(esp_reset_reason_t reason);

#endif /* __MAIN_H__ */
