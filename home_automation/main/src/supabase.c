// #include "supabase.h"
// #include "esp_wifi.h"
// #include "esp_timer.h"
// #include "driver/gpio.h"
// #include "esp_crt_bundle.h"
// #define TAG_DB "Supabase"
// #include "esp_websocket_client.h" 
// // Global array to store local states (matches IDs 1-9 in DB)
// int relay_states[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};

// // GPIO Mapping based on your provided image
// const int RELAY_GPIOS[9] = {13, 14, 4, 16, 17, 5, 18, 19, 21};

// esp_websocket_client_handle_t ws_client;

// // Helper: Get MAC Address as string for Device ID
// void get_mac_address(char *mac_str) {
//     uint8_t mac[6];
//     esp_efuse_mac_get_default(mac);
//     sprintf(mac_str, "%02X:%02X:%02X:%02X:%02X:%02X", 
//             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
// }

//  * WebSocket Event Handler: This catches the "Update" from the server
//  */
// static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
// esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
//     switch (event_id) {
//         case WEBSOCKET_EVENT_CONNECTED:
//             ESP_LOGI(TAG_DB, "WebSocket Connected");
//             const char *join_msg = "{\"topic\":\"realtime:public:relays\",\"event\":\"phx_join\",\"payload\":{},\"ref\":\"1\"}";
//             esp_websocket_client_send_text(ws_client, join_msg, strlen(join_msg), portMAX_DELAY);
//             break;
//         case WEBSOCKET_EVENT_DATA:
//             if (data->data_ptr != NULL) {
//                 ESP_LOGI(TAG_DB, "Received Realtime Update: %.*s", data->data_len, (char *)data->data_ptr);
                
//                 cJSON *root = cJSON_ParseWithLength(data->data_ptr, data->data_len);
//                 if (root) {
//                     cJSON *event = cJSON_GetObjectItem(root, "event");
//                     // Check if it's an UPDATE event from the DB
//                     if (event && strcmp(event->valuestring, "UPDATE") == 0) {
//                         cJSON *payload = cJSON_GetObjectItem(root, "payload");
//                         cJSON *record = cJSON_GetObjectItem(payload, "record");
//                         if (record) {
//                             int id = cJSON_GetObjectItem(record, "id")->valueint;
//                             int state = cJSON_GetObjectItem(record, "state")->valueint;
                            
//                             if (id >= 1 && id <= 9) {
//                                 int idx = id - 1;
//                                 relay_states[idx] = state;
//                                 gpio_set_level(RELAY_GPIOS[idx], state);
//                                 ESP_LOGW(TAG_DB, "INSTANT UPDATE: Relay %d -> %d", id, state);
//                             }
//                         }
//                     }
//                     cJSON_Delete(root);
//                 }
//             }
//             break;

//         case WEBSOCKET_EVENT_DISCONNECTED:
//             ESP_LOGE(TAG_DB, "WebSocket Disconnected");
//             break;
//     }
// }

// /**
//  * Initializes the WebSocket Connection
//  */

// void start_supabase_realtime(void) {
//     // 1. Construct WebSocket URL
//     // Format: wss://[project].supabase.co/realtime/v1/websocket?apikey=[key]&vsn=1.0.0
//     char ws_url[1024];
//     snprintf(ws_url, sizeof(ws_url), "wss://hbpwmqxwnnffgnncbgeq.supabase.co/realtime/v1/websocket?apikey=%s&vsn=1.0.0", SUPABASE_ANON_KEY);

//     esp_websocket_client_config_t ws_cfg = {
//         .uri = ws_url,
//     };

//     ESP_LOGI(TAG_DB, "Connecting to Realtime...");
//     ws_client = esp_websocket_client_init(&ws_cfg);
//     esp_websocket_register_events(ws_client, WEBSOCKET_EVENT_ANY, websocket_event_handler, NULL);
//     esp_websocket_client_start(ws_client);
// }

// // REST Function: Fetch initial state (Keep this as is)


// /**
//  * @brief Fetches state for all 9 relays. 
//  * Use this on boot to "remember" previous states.
//  */
// void fetch_relay_states_from_supabase(void) {
//     ESP_LOGI(TAG_DB, "Fetching relay states for retention...");

//     char url[256];
//     snprintf(url, sizeof(url), "%s/rest/v1/%s?select=id,state&order=id.asc", SUPABASE_URL, TABLE_RELAYS);

//     esp_http_client_config_t config = {
//         .url = url,
//         .method = HTTP_METHOD_GET,
//         .timeout_ms = 10000,
//         .buffer_size = 2048,     // <--- ADD THIS: Increase RX buffer for headers
//         .buffer_size_tx = 1024, 
//     };
//     esp_http_client_handle_t client = esp_http_client_init(&config);

//     esp_http_client_set_header(client, "apikey", SUPABASE_ANON_KEY);
//     char bearer[512];
//     snprintf(bearer, sizeof(bearer), "Bearer %s", SUPABASE_ANON_KEY);
//     esp_http_client_set_header(client, "Authorization", bearer);

//     esp_err_t err = esp_http_client_open(client, 0);
//     if (err == ESP_OK) {
//         esp_http_client_fetch_headers(client);
//         char *buffer = malloc(2048);
//         int read_len = esp_http_client_read_response(client, buffer, 2047);
//         if (read_len > 0) {
//             buffer[read_len] = '\0';
//             cJSON *root = cJSON_Parse(buffer);
//             if (cJSON_IsArray(root)) {
//                 int size = cJSON_GetArraySize(root);
//                 for (int i = 0; i < size && i < 9; i++) {
//                     cJSON *item = cJSON_GetArrayItem(root, i);
//                     int db_state = cJSON_GetObjectItem(item, "state")->valueint;
                    
//                     // Apply to hardware immediately
//                     relay_states[i] = db_state;
//                     gpio_set_level(RELAY_GPIOS[i], db_state);
//                     ESP_LOGI(TAG_DB, "Relay %d (GPIO %d) set to %d", i+1, RELAY_GPIOS[i], db_state);
//                 }
//             }
//             cJSON_Delete(root);
//         }
//         free(buffer);
//     }
//     esp_http_client_cleanup(client);
// }

// /**
//  * @brief Updates a specific relay state in the DB when a physical switch is toggled.
//  */
// void update_relay_state_in_supabase(int relay_id, int new_state) {
//     char url[256];
//     // Target the specific row (id is 1-indexed in our SQL setup)
//     snprintf(url, sizeof(url), "%s/rest/v1/%s?id=eq.%d", SUPABASE_URL, TABLE_RELAYS, relay_id);

//     cJSON *root = cJSON_CreateObject();
//     cJSON_AddNumberToObject(root, "state", new_state);
//     char *json_string = cJSON_PrintUnformatted(root);

//     esp_http_client_config_t config = {
//         .url = url,
//         .method = HTTP_METHOD_PATCH,
//         .timeout_ms = 10000,
//         .buffer_size = 2048,     // <--- ADD THIS
//         .buffer_size_tx = 1024, 
//         // .crt_bundle_attach = esp_crt_bundle_attach,
//     };
//     esp_http_client_handle_t client = esp_http_client_init(&config);

//     esp_http_client_set_header(client, "apikey", SUPABASE_ANON_KEY);
//     esp_http_client_set_header(client, "Content-Type", "application/json");
//     char bearer[512];
//     snprintf(bearer, sizeof(bearer), "Bearer %s", SUPABASE_ANON_KEY);
//     esp_http_client_set_header(client, "Authorization", bearer);

//     esp_http_client_set_post_field(client, json_string, strlen(json_string));

//     esp_err_t err = esp_http_client_perform(client);
//     if (err == ESP_OK) {
//         ESP_LOGI(TAG_DB, "Relay %d updated to %d in DB", relay_id, new_state);
//     }

//     esp_http_client_cleanup(client);
//     cJSON_Delete(root);
//     free(json_string);
// }

// /**
//  * @brief Sends Device Health (RSSI/Uptime) to the 'device_status' table.
//  */
// void send_device_health_to_supabase(void) {
//     char mac_str[18];
//     get_mac_address(mac_str);

//     wifi_ap_record_t ap;
//     esp_wifi_sta_get_ap_info(&ap);

//     cJSON *root = cJSON_CreateObject();
//     cJSON_AddStringToObject(root, "device_id", mac_str);
//     cJSON_AddNumberToObject(root, "wifi_rssi", ap.rssi);
//     cJSON_AddNumberToObject(root, "uptime_seconds", esp_timer_get_time() / 1000000);
//     cJSON_AddStringToObject(root, "firmware_version", "1.1.0 (Realtime)");

//     char *json_string = cJSON_PrintUnformatted(root);

//     char url[256];
//     // Use upsert logic (on_conflict) so it updates the existing device_id row
//     snprintf(url, sizeof(url), "%s/rest/v1/%s", SUPABASE_URL, TABLE_DEVICE_STATUS);

//     esp_http_client_config_t config = {
//         .url = url,
//         .method = HTTP_METHOD_POST,
//         .buffer_size = 2048,     // <--- ADD THIS
//         .buffer_size_tx = 1024, 
//         // .crt_bundle_attach = esp_crt_bundle_attach,
//         // .skip_cert_common_name_check = true, 
//     };
//     esp_http_client_handle_t client = esp_http_client_init(&config);

//     esp_http_client_set_header(client, "apikey", SUPABASE_ANON_KEY);
//     esp_http_client_set_header(client, "Content-Type", "application/json");
//     esp_http_client_set_header(client, "Prefer", "resolution=merge-duplicates"); // Upsert
    
//     char bearer[512];
//     snprintf(bearer, sizeof(bearer), "Bearer %s", SUPABASE_ANON_KEY);
//     esp_http_client_set_header(client, "Authorization", bearer);

//     esp_http_client_set_post_field(client, json_string, strlen(json_string));
//     esp_http_client_perform(client);

//     esp_http_client_cleanup(client);
//     cJSON_Delete(root);
//     free(json_string);
// }

// /**
//  * @brief Background task
//  */
// void supabase_sync_task(void *pvParameters) {
//     // 1. On boot: Fetch previous states to restore relays
//     fetch_relay_states_from_supabase();

//         // 2. Start WebSocket Realtime
//     start_supabase_realtime();

//     uint32_t last_health = 0;
//     uint32_t last_heartbeat = 0;



//     while(1) {
//         // // 2. Every 30 seconds: Update device health/heartbeat
//         // send_device_health_to_supabase();
        
//         // // 3. Every 5 seconds: Poll for remote changes from Dashboard/App
//         // fetch_relay_states_from_supabase();
        
//         // vTaskDelay(pdMS_TO_TICKS(5000)); 


//         uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

//         // Phoenix Heartbeat (Required every 30s to keep socket alive)
//         if (now - last_heartbeat >= 30000) {
//             if (esp_websocket_client_is_connected(ws_client)) {
//                 const char *hb = "{\"topic\":\"phoenix\",\"event\":\"heartbeat\",\"payload\":{},\"ref\":\"2\"}";
//                 esp_websocket_client_send_text(ws_client, hb, strlen(hb), portMAX_DELAY);
//                 ESP_LOGD(TAG_DB, "WebSocket Heartbeat sent");
//             }
//             last_heartbeat = now;
//         }

//         // Send Device Health every 60 seconds
//         if (now - last_health >= 60000) {
//             send_device_health_to_supabase();
//             last_health = now;
//         }

//         vTaskDelay(pdMS_TO_TICKS(1000)); 
//     }
// }


#include "supabase.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "esp_crt_bundle.h"
#include "esp_websocket_client.h"
#include "esp_system.h"
#include "esp_wifi.h"

#define TAG_DB "Supabase"

// Global and Static Variables
int relay_states[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
const int RELAY_GPIOS[9] = {13, 14, 4, 16, 17, 5, 18, 19, 21};
static esp_websocket_client_handle_t ws_client = NULL;

// Forward Declaration of the handler
static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

/**
 * Helper: Get MAC Address as string
 */
void get_mac_address(char *mac_str) {
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    sprintf(mac_str, "%02X:%02X:%02X:%02X:%02X:%02X", 
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

/**
 * WebSocket Event Handler
 */
static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    
    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG_DB, "WebSocket Connected. Sending Join Message...");
            const char *join_msg = "{\"topic\":\"realtime:public:relays\",\"event\":\"phx_join\",\"payload\":{},\"ref\":\"1\"}";
            esp_websocket_client_send_text(ws_client, join_msg, strlen(join_msg), portMAX_DELAY);
            break;

        case WEBSOCKET_EVENT_DATA:
            if (data->data_ptr != NULL && data->data_len > 0) {
                cJSON *root = cJSON_ParseWithLength(data->data_ptr, data->data_len);
                if (root) {
                    cJSON *event = cJSON_GetObjectItem(root, "event");
                    if (event && event->valuestring && strcmp(event->valuestring, "UPDATE") == 0) {
                        cJSON *payload = cJSON_GetObjectItem(root, "payload");
                        cJSON *record = cJSON_GetObjectItem(payload, "record");
                        if (record) {
                            int id = cJSON_GetObjectItem(record, "id")->valueint;
                            int state = cJSON_GetObjectItem(record, "state")->valueint;
                            if (id >= 1 && id <= 9) {
                                int idx = id - 1;
                                relay_states[idx] = state;
                                gpio_set_level(RELAY_GPIOS[idx], state);
                                ESP_LOGW(TAG_DB, "REALTIME: Relay %d set to %d", id, state);
                            }
                        }
                    }
                    cJSON_Delete(root);
                }
            }
            break;

        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGE(TAG_DB, "WebSocket Disconnected");
            break;
    }
}

/**
 * Start Supabase Realtime Service
 */
void start_supabase_realtime(void) {
    char ws_url[1024];
    snprintf(ws_url, sizeof(ws_url), 
             "wss://hbpwmqxwnnffgnncbgeq.supabase.co/realtime/v1/websocket?apikey=%s&vsn=1.0.0", 
             SUPABASE_ANON_KEY);

    esp_websocket_client_config_t ws_cfg = {
        .uri = ws_url,
    };

    ESP_LOGI(TAG_DB, "Connecting to Supabase Realtime...");
    ws_client = esp_websocket_client_init(&ws_cfg);
    esp_websocket_register_events(ws_client, WEBSOCKET_EVENT_ANY, websocket_event_handler, NULL);
    esp_websocket_client_start(ws_client);
}

/**
 * @brief Updates a specific relay state in the DB when a physical switch is toggled.
 */
void update_relay_state_in_supabase(int relay_id, int new_state) {
    char url[256];
    // Target the specific row (id is 1-indexed)
    snprintf(url, sizeof(url), "%s/rest/v1/%s?id=eq.%d", SUPABASE_URL, TABLE_RELAYS, relay_id);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "state", new_state);
    char *json_string = cJSON_PrintUnformatted(root);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_PATCH,
        .timeout_ms = 10000,
        .buffer_size = SB_HTTP_BUFFER_SIZE,
        .buffer_size_tx = SB_HTTP_BUFFER_SIZE,
        // .header_buffer_size = 8192,   
        .crt_bundle_attach = esp_crt_bundle_attach,
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
        ESP_LOGI(TAG_DB, "Relay %d updated to %d in Supabase (REST)", relay_id, new_state);
    } else {
        ESP_LOGE(TAG_DB, "Failed to update relay in Supabase: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    cJSON_Delete(root);
    free(json_string);
}

/**
 * Fetch states on boot (REST)
 */
void fetch_relay_states_from_supabase(void) {
    char url[256];
    snprintf(url, sizeof(url), "%s/rest/v1/%s?select=id,state&order=id.asc", SUPABASE_URL, TABLE_RELAYS);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 10000,
        .buffer_size = SB_HTTP_BUFFER_SIZE,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "apikey", SUPABASE_ANON_KEY);
    char bearer[512];
    snprintf(bearer, sizeof(bearer), "Bearer %s", SUPABASE_ANON_KEY);
    esp_http_client_set_header(client, "Authorization", bearer);

    if (esp_http_client_open(client, 0) == ESP_OK) {
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
                    relay_states[i] = db_state;
                    gpio_set_level(RELAY_GPIOS[i], db_state);
                }
            }
            cJSON_Delete(root);
        }
        free(buffer);
    }
    esp_http_client_cleanup(client);
}

/**
 * Send Health Report (REST)
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
    cJSON_AddStringToObject(root, "firmware_version", "1.1.0-WebSocket");

    char *json_string = cJSON_PrintUnformatted(root);
    char url[256];
    snprintf(url, sizeof(url), "%s/rest/v1/%s", SUPABASE_URL, TABLE_DEVICE_STATUS);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .buffer_size = SB_HTTP_BUFFER_SIZE,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "apikey", SUPABASE_ANON_KEY);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Prefer", "resolution=merge-duplicates");
    char bearer[4096];
    snprintf(bearer, sizeof(bearer), "Bearer %s", SUPABASE_ANON_KEY);
    esp_http_client_set_header(client, "Authorization", bearer);

    esp_http_client_set_post_field(client, json_string, strlen(json_string));
    esp_http_client_perform(client);

    esp_http_client_cleanup(client);
    cJSON_Delete(root);
    free(json_string);
}

/**
 * Background Sync Task
 */
void supabase_sync_task(void *pvParameters) {
    // 1. Fetch ONCE at boot to get initial state
    fetch_relay_states_from_supabase();
    
    // 2. Connect Realtime
    start_supabase_realtime();

    uint32_t last_health = 0;
    uint32_t last_heartbeat = 0;

    while(1) {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

        // WebSocket Heartbeat every 30s
        if (now - last_heartbeat >= 30000) {
            if (ws_client && esp_websocket_client_is_connected(ws_client)) {
                const char *hb = "{\"topic\":\"phoenix\",\"event\":\"heartbeat\",\"payload\":{},\"ref\":\"2\"}";
                esp_websocket_client_send_text(ws_client, hb, strlen(hb), portMAX_DELAY);
            }
            last_heartbeat = now;
        }

        // REST Health Update every 60s
        if (now - last_health >= 60000) {
            send_device_health_to_supabase();
            last_health = now;
        }

        vTaskDelay(pdMS_TO_TICKS(1000)); 
    }
}