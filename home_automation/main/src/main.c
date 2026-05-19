#include "main.h"
#include "wifi.h"
#include "supabase.h"
#include <time.h>

#define TAG_1 "Provisoning"
#define TAG_INFO "Provisoning"
#define TAG "Main"
struct timeval tv;
extern float ultrasonic_data;
extern uint32_t tick;
const char* G_REBOOT_REASON_STR = "Unknown";
void app_main()
{
    esp_reset_reason_t reboot_reason = esp_reset_reason();
    G_REBOOT_REASON_STR = get_reboot_reason_string(reboot_reason);
    ESP_LOGI(TAG, "Last reboot reason: %s", G_REBOOT_REASON_STR);

	print_system_memory_status();
    setup_gpios();
    connect_wifi();
    initialize_sntp();

    xTaskCreate(&supabase_sync_task, "Device Control Task", 8192, NULL, 5, NULL);
	
    xTaskCreate(&heartbeat_task, "Heartbeat Task", 8192, NULL, 3, NULL);
}

void print_system_memory_status() 
{
    ESP_LOGI(TAG_INFO, "========== Chip Information ===========================================");
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    const char *chip_model;
    switch (chip_info.model) {
        case CHIP_ESP32: chip_model = "ESP32"; break;
        case CHIP_ESP32S2: chip_model = "ESP32-S2"; break;
        case CHIP_ESP32S3: chip_model = "ESP32-S3"; break;
        case CHIP_ESP32C3: chip_model = "ESP32-C3"; break;
        case CHIP_ESP32H2: chip_model = "ESP32-H2"; break;
        default: chip_model = "Unknown"; break;
    }
    ESP_LOGI(TAG_INFO, "Chip model: %s", chip_model);

    ESP_LOGI(TAG_INFO,"CPU cores: %d", chip_info.cores);
    ESP_LOGI(TAG_INFO,"Silicon revision: %d", chip_info.revision);

    uint32_t flash_size = 0;
    esp_flash_get_size(NULL, &flash_size);  // <-- updated function
    ESP_LOGI(TAG_INFO,"Flash size: %lu MB", (unsigned long) flash_size / (1024 * 1024));

    const esp_app_desc_t *app_desc = esp_app_get_description();
    gettimeofday(&tv, NULL);
    ESP_LOGI(TAG_INFO, "========== Program Version ============================================");
    ESP_LOGI(TAG_INFO, "[APP] Name: %s", app_desc->project_name);
    
      ESP_LOGI(TAG_INFO, "[APP] Version: %s", app_desc->version);
    ESP_LOGI(TAG_INFO, "[APP] Compile Date: %s", app_desc->date);
    ESP_LOGI(TAG_INFO, "[APP] Compile Time: %s", app_desc->time);
    ESP_LOGI(TAG_INFO, "========== Heap Information ===========================================");
    ESP_LOGI(TAG_INFO,"Total free heap: %lu bytes", (unsigned long) heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
    ESP_LOGI(TAG_INFO,"Minimum free heap since boot: %lu bytes", (unsigned long) heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT));
    ESP_LOGI(TAG_INFO,"Internal RAM free: %lu bytes", (unsigned long) heap_caps_get_free_size(MALLOC_CAP_INTERNAL));

    ESP_LOGI(TAG_INFO, "========== Stack Information ==========================================");
    ESP_LOGI(TAG_INFO,"Current task stack high water mark: %lu bytes", (unsigned long) uxTaskGetStackHighWaterMark(NULL));

    ESP_LOGI(TAG_INFO, "========== Flash Partition Information ================================");
    const esp_partition_t *part = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, NULL);
    if (part != NULL) {
        ESP_LOGI(TAG_INFO,"App partition size: %lu bytes", (unsigned long) part->size);
    } else {
        ESP_LOGI(TAG_INFO,"App partition not found!");
    }

    ESP_LOGI(TAG_INFO, "=======================================================================");
}

void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();

    // Wait for time to be set
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 15;

    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }

    time(&now);
    localtime_r(&now, &timeinfo);

    if (timeinfo.tm_year < (2023 - 1900)) {
        ESP_LOGE(TAG, "Time synchronization failed!");
        // In a real app, you might want to retry or restart here
    } else {
        ESP_LOGI(TAG, "System time is set.");
    }
}

/**
 * @brief A dedicated FreeRTOS task to send a heartbeat every 60 seconds.
 */
void heartbeat_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Heartbeat task started.");

    while(1) {
        ESP_LOGW(TAG, "[Heartbeat Task] Sending device heartbeat...");
        update_device_status();
        
        // Wait for 60 seconds before sending the next heartbeat
        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}

/**
 * @brief Sends a detailed heartbeat/status report to the device_status table.
 */
void update_device_status(void)
{
    // This global variable is set once in app_main.
    extern const char* G_REBOOT_REASON_STR; 

    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    cJSON *root = cJSON_CreateObject();

    // 1. Add the primary key
    cJSON_AddStringToObject(root, "device_id", mac_str);

    // 2. Add the Wi-Fi RSSI
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        cJSON_AddNumberToObject(root, "wifi_rssi", ap_info.rssi);
    }

    // 3. Add the Uptime
    long long uptime_seconds = esp_timer_get_time() / 1000000;
    cJSON_AddNumberToObject(root, "uptime_seconds", uptime_seconds);

    // 4. Add the Firmware Version
    const esp_app_desc_t *app_desc = esp_app_get_description();
    cJSON_AddStringToObject(root, "firmware_version", app_desc->version);
    
    // 5. Add the Last Reboot Reason
    cJSON_AddStringToObject(root, "last_reboot_reason", G_REBOOT_REASON_STR);

    // 6. --- NEW: Get and add the current epoch time ---
    time_t now;
    time(&now); // 'now' now holds the epoch time in seconds
    cJSON_AddNumberToObject(root, "device_timestamp", now);

    char *json_string = cJSON_PrintUnformatted(root);
    ESP_LOGI(TAG, "Heartbeat Payload: %s", json_string);
    
    char url[256];
    snprintf(url, sizeof(url), "%s/rest/v1/device_status", SUPABASE_URL);
    
    // (The rest of the HTTP client logic remains exactly the same)
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 10000,
        .buffer_size_tx = 1024,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    
    char bearer[256];
    snprintf(bearer, sizeof(bearer), "Bearer %s", SUPABASE_ANON_KEY);
    esp_http_client_set_header(client, "apikey", SUPABASE_ANON_KEY);
    esp_http_client_set_header(client, "Authorization", bearer);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Prefer", "resolution=merge-duplicates");

    esp_http_client_set_post_field(client, json_string, strlen(json_string));
    
    if (esp_http_client_perform(client) == ESP_OK) {
        ESP_LOGI(TAG, "Heartbeat sent successfully, status = %d", esp_http_client_get_status_code(client));
    } else {
        ESP_LOGE(TAG, "Heartbeat failed.");
    }

    esp_http_client_cleanup(client);
    cJSON_Delete(root);
    free(json_string);
}

/**
 * @brief Converts the esp_reset_reason_t enum to a human-readable string.
 */
static const char* get_reboot_reason_string(esp_reset_reason_t reason)
{
    switch (reason) {
        case ESP_RST_POWERON:      return "Power On";
        case ESP_RST_SW:           return "Software Reset";
        case ESP_RST_PANIC:        return "Panic/Crash";
        case ESP_RST_INT_WDT:      return "Interrupt Watchdog";
        case ESP_RST_TASK_WDT:     return "Task Watchdog";
        case ESP_RST_DEEPSLEEP:    return "Deep Sleep Wakeup";
        case ESP_RST_BROWNOUT:     return "Brownout (Low Voltage)";
        default:                   return "Unknown";
    }
}