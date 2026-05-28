#include "main.h"
#include "wifi.h"
#include "supabase.h"
#include <time.h>
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_crt_bundle.h"

#define TAG "Main"
struct timeval tv;
extern float ultrasonic_data;
extern uint32_t tick;
const char* G_REBOOT_REASON_STR = "Unknown";

/**
 * @brief Converts the esp_reset_reason_t enum to a human-readable string.
 */
const char* get_reboot_reason_string(esp_reset_reason_t reason)
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

void app_main()
{
    esp_reset_reason_t reboot_reason = esp_reset_reason();
    G_REBOOT_REASON_STR = get_reboot_reason_string(reboot_reason);
    ESP_LOGI(TAG, "Last reboot reason: %s", G_REBOOT_REASON_STR);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // If NVS partition was truncated or has a new version, erase it and re-init
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGW("DEBUG", "=============================================");
    ESP_LOGW("DEBUG", "HOME AUTOMATION Project...(OTA & Realtime Enabled)(%s)", FIRMWARE_VERSION);
    ESP_LOGW("DEBUG", "=============================================");
	print_system_memory_status();
    setup_gpios();
    load_relay_states_from_nvs();
    ESP_LOGW("STARTUP", "Relay 1 is currently: %d", relay_states[0]);
    ESP_LOGW("DEBUG", "NVS RESTORE COMPLETE. Starting Network...");
    vTaskDelay(pdMS_TO_TICKS(2000)); // Short delay before starting Wi-Fi

    connect_wifi();
    vTaskDelay(pdMS_TO_TICKS(5000)); // Wait 5 seconds for IP
    initialize_sntp();
    handle_provisioning_timestamp();
    xTaskCreate(&supabase_sync_task, "Supabase Sync", 16384, NULL, 5, NULL);
	
    xTaskCreate(&heartbeat_task, "Heartbeat Task", 16384, NULL, 3, NULL);
}

void print_system_memory_status() 
{
    ESP_LOGI(TAG, "========== Chip Information ===========================================");
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
    ESP_LOGI(TAG, "Chip model: %s", chip_model);

    ESP_LOGI(TAG,"CPU cores: %d", chip_info.cores);
    ESP_LOGI(TAG,"Silicon revision: %d", chip_info.revision);

    uint32_t flash_size = 0;
    esp_flash_get_size(NULL, &flash_size);
    ESP_LOGI(TAG,"Flash size: %lu MB", (unsigned long) flash_size / (1024 * 1024));

    const esp_app_desc_t *app_desc = esp_app_get_description();
    gettimeofday(&tv, NULL);
    ESP_LOGI(TAG, "========== Program Version ============================================");
    ESP_LOGI(TAG, "[APP] Name: %s", app_desc->project_name);
    
      ESP_LOGI(TAG, "[APP] Version: %s", app_desc->version);
    ESP_LOGI(TAG, "[APP] Compile Date: %s", app_desc->date);
    ESP_LOGI(TAG, "[APP] Compile Time: %s", app_desc->time);
    ESP_LOGI(TAG, "========== Heap Information ===========================================");
    ESP_LOGI(TAG,"Total free heap: %lu bytes", (unsigned long) heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
    ESP_LOGI(TAG,"Minimum free heap since boot: %lu bytes", (unsigned long) heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT));
    ESP_LOGI(TAG,"Internal RAM free: %lu bytes", (unsigned long) heap_caps_get_free_size(MALLOC_CAP_INTERNAL));

    ESP_LOGI(TAG, "========== Stack Information ==========================================");
    ESP_LOGI(TAG,"Current task stack high water mark: %lu bytes", (unsigned long) uxTaskGetStackHighWaterMark(NULL));

    ESP_LOGI(TAG, "========== Flash Partition Information ================================");
    const esp_partition_t *part = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, NULL);
    if (part != NULL) {
        ESP_LOGI(TAG,"App partition size: %lu bytes", (unsigned long) part->size);
    } else {
        ESP_LOGI(TAG,"App partition not found!");
    }

    ESP_LOGI(TAG, "=======================================================================");
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
        ESP_LOGE(TAG, "Time synchronization failed! Restarting in 5 seconds...");
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_restart();

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

// Helper to manage the Provisioned Timestamp in NVS
void handle_provisioning_timestamp() {
    nvs_handle_t handle;
    uint8_t new_prov = 0;
    int64_t prov_time = 0;

    if (nvs_open("storage", NVS_READWRITE, &handle) == ESP_OK) {
        // Check if we just provisioned
        nvs_get_u8(handle, "new_prov", &new_prov);
        
        if (new_prov == 1) {
            time_t now;
            time(&now); // Get current synced time
            prov_time = (int64_t)now;
            
            nvs_set_i64(handle, "prov_ts", prov_time); // Save actual timestamp
            nvs_set_u8(handle, "new_prov", 0);         // Clear flag
            nvs_commit(handle);
            ESP_LOGW(TAG, "Recorded new provisioning time: %lld", prov_time);
        }
        nvs_close(handle);
    }
}

/**
 * @brief Sends a detailed heartbeat/status report including WiFi credentials and provisioning date
 */
void update_device_status(void)
{
    extern const char* G_REBOOT_REASON_STR; 
    nvs_handle_t handle;
    int64_t last_prov_ts = 0;

    // 1. Get MAC Address (Primary Key)
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X", 
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    // 2. Get WiFi SSID (not password for security)
    wifi_config_t wifi_cfg;
    char current_ssid[33] = {0};
    if (esp_wifi_get_config(WIFI_IF_STA, &wifi_cfg) == ESP_OK) {
        strncpy(current_ssid, (char*)wifi_cfg.sta.ssid, sizeof(current_ssid) - 1);
    }

    // 3. Read Provisioned Time from NVS (if it exists)
    if (nvs_open("storage", NVS_READONLY, &handle) == ESP_OK) {
        nvs_get_i64(handle, "prov_ts", &last_prov_ts);
        nvs_close(handle);
    }

    // 4. Create the JSON Payload
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "device_id", mac_str);
    cJSON_AddStringToObject(root, "wifi_ssid", current_ssid);

    // Add Signal Strength
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        cJSON_AddNumberToObject(root, "wifi_rssi", ap_info.rssi);
    }

    // Add Uptime
    long long uptime_seconds = esp_timer_get_time() / 1000000;
    cJSON_AddNumberToObject(root, "uptime_seconds", uptime_seconds);

    // Add Firmware & Reboot Reason
    cJSON_AddStringToObject(root, "firmware_version", FIRMWARE_VERSION);
    cJSON_AddStringToObject(root, "last_reboot_reason", G_REBOOT_REASON_STR);

    // Handle Provisioned Date string
    if (last_prov_ts > 0) {
        char prov_time_str[32];
        struct tm timeinfo;
        time_t t = (time_t)last_prov_ts;
        gmtime_r(&t, &timeinfo);
        strftime(prov_time_str, sizeof(prov_time_str), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
        cJSON_AddStringToObject(root, "last_provisioned_at", prov_time_str);
    } else {
        cJSON_AddNullToObject(root, "last_provisioned_at");
    }
    
    // Add current device heartbeat timestamp
    time_t now;
    time(&now);
    cJSON_AddNumberToObject(root, "device_timestamp", (long long)now);

    char *json_string = cJSON_PrintUnformatted(root);
    ESP_LOGI(TAG, "Heartbeat Payload: %s", json_string);
    
    // 5. HTTP POST Configuration
    char url[256];
    snprintf(url, sizeof(url), "%s/rest/v1/device_status", SUPABASE_URL);
    
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 10000,
        .buffer_size = SB_HTTP_BUFFER_SIZE,
        .buffer_size_tx = SB_HTTP_BUFFER_SIZE,
    .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    
    char bearer[8192];
    snprintf(bearer, sizeof(bearer), "Bearer %s", SUPABASE_ANON_KEY);
    esp_http_client_set_header(client, "apikey", SUPABASE_ANON_KEY);
    esp_http_client_set_header(client, "Authorization", bearer);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Prefer", "resolution=merge-duplicates"); // UPSERT

    esp_http_client_set_post_field(client, json_string, strlen(json_string));
    
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Heartbeat sent successfully (SSID: %s)", current_ssid);
    } else {
        ESP_LOGE(TAG, "Heartbeat failed: %s", esp_err_to_name(err));
    }

    // 6. Cleanup
    esp_http_client_cleanup(client);
    cJSON_Delete(root); // Deletes root and all child items
    free(json_string);
}
