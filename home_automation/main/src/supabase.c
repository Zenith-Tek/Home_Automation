#include "supabase.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "esp_crt_bundle.h"
#define TAG_DB "Supabase"

// Global array to store local states (matches IDs 1-9 in DB)
int relay_states[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};

// GPIO Mapping based on your provided image
const int RELAY_GPIOS[9] = {13, 14, 4, 16, 17, 5, 18, 19, 21};

// Helper: Get MAC Address as string for Device ID
void get_mac_address(char *mac_str) {
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    sprintf(mac_str, "%02X:%02X:%02X:%02X:%02X:%02X", 
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

/**
 * @brief Fetches state for all 9 relays. 
 * Use this on boot to "remember" previous states.
 */
void fetch_relay_states_from_supabase(void) {
    ESP_LOGI(TAG_DB, "Fetching relay states for retention...");

    char url[256];
    snprintf(url, sizeof(url), "%s/rest/v1/%s?select=id,state&order=id.asc", SUPABASE_URL, TABLE_RELAYS);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 10000,
        .buffer_size = 2048,     // <--- ADD THIS: Increase RX buffer for headers
        .buffer_size_tx = 1024, 
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_header(client, "apikey", SUPABASE_ANON_KEY);
    char bearer[512];
    snprintf(bearer, sizeof(bearer), "Bearer %s", SUPABASE_ANON_KEY);
    esp_http_client_set_header(client, "Authorization", bearer);

    esp_err_t err = esp_http_client_open(client, 0);
    if (err == ESP_OK) {
        esp_http_client_fetch_headers(client);
        char *buffer = malloc(2048);
        int read_len = esp_http_client_read_response(client, buffer, 2047);
        if (read_len > 0) {
            buffer[read_len] = '\0';
            cJSON *root = cJSON_Parse(buffer);
            if (cJSON_IsArray(root)) {
                int size = cJSON_GetArraySize(root);
                for (int i = 0; i < size && i < 9; i++) {
                    cJSON *item = cJSON_GetArrayItem(root, i);
                    int db_state = cJSON_GetObjectItem(item, "state")->valueint;
                    
                    // Apply to hardware immediately
                    relay_states[i] = db_state;
                    gpio_set_level(RELAY_GPIOS[i], db_state);
                    ESP_LOGI(TAG_DB, "Relay %d (GPIO %d) set to %d", i+1, RELAY_GPIOS[i], db_state);
                }
            }
            cJSON_Delete(root);
        }
        free(buffer);
    }
    esp_http_client_cleanup(client);
}

/**
 * @brief Updates a specific relay state in the DB when a physical switch is toggled.
 */
void update_relay_state_in_supabase(int relay_id, int new_state) {
    char url[256];
    // Target the specific row (id is 1-indexed in our SQL setup)
    snprintf(url, sizeof(url), "%s/rest/v1/%s?id=eq.%d", SUPABASE_URL, TABLE_RELAYS, relay_id);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "state", new_state);
    char *json_string = cJSON_PrintUnformatted(root);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_PATCH,
        .timeout_ms = 10000,
        .buffer_size = 2048,     // <--- ADD THIS
        .buffer_size_tx = 1024, 
        // .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_header(client, "apikey", SUPABASE_ANON_KEY);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    char bearer[512];
    snprintf(bearer, sizeof(bearer), "Bearer %s", SUPABASE_ANON_KEY);
    esp_http_client_set_header(client, "Authorization", bearer);

    esp_http_client_set_post_field(client, json_string, strlen(json_string));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG_DB, "Relay %d updated to %d in DB", relay_id, new_state);
    }

    esp_http_client_cleanup(client);
    cJSON_Delete(root);
    free(json_string);
}

/**
 * @brief Sends Device Health (RSSI/Uptime) to the 'device_status' table.
 */
void send_device_health_to_supabase(void) {
    char mac_str[18];
    get_mac_address(mac_str);

    wifi_ap_record_t ap;
    esp_wifi_sta_get_ap_info(&ap);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "device_id", mac_str);
    cJSON_AddNumberToObject(root, "wifi_rssi", ap.rssi);
    cJSON_AddNumberToObject(root, "uptime_seconds", esp_timer_get_time() / 1000000);
    cJSON_AddStringToObject(root, "firmware_version", "1.0.0");

    char *json_string = cJSON_PrintUnformatted(root);

    char url[256];
    // Use upsert logic (on_conflict) so it updates the existing device_id row
    snprintf(url, sizeof(url), "%s/rest/v1/%s", SUPABASE_URL, TABLE_DEVICE_STATUS);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .buffer_size = 2048,     // <--- ADD THIS
        .buffer_size_tx = 1024, 
        // .crt_bundle_attach = esp_crt_bundle_attach,
        // .skip_cert_common_name_check = true, 
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_header(client, "apikey", SUPABASE_ANON_KEY);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Prefer", "resolution=merge-duplicates"); // Upsert
    
    char bearer[512];
    snprintf(bearer, sizeof(bearer), "Bearer %s", SUPABASE_ANON_KEY);
    esp_http_client_set_header(client, "Authorization", bearer);

    esp_http_client_set_post_field(client, json_string, strlen(json_string));
    esp_http_client_perform(client);

    esp_http_client_cleanup(client);
    cJSON_Delete(root);
    free(json_string);
}

/**
 * @brief Background task
 */
void supabase_sync_task(void *pvParameters) {
    // 1. On boot: Fetch previous states to restore relays
    fetch_relay_states_from_supabase();

    while(1) {
        // 2. Every 30 seconds: Update device health/heartbeat
        send_device_health_to_supabase();
        
        // 3. Every 5 seconds: Poll for remote changes from Dashboard/App
        fetch_relay_states_from_supabase();
        
        vTaskDelay(pdMS_TO_TICKS(5000)); 
    }
}