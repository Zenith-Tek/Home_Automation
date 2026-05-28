#include "supabase.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "esp_crt_bundle.h"
#include "esp_websocket_client.h"
#include "esp_system.h"
#include "esp_https_ota.h"
#include "nvs_flash.h"
#include "nvs.h"

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
void ota_task(void *pvParameter) {
    char *url = (char *)pvParameter;
    ESP_LOGW(TAG_DB, "OTA TASK: Downloading firmware from %s", url);

    esp_http_client_config_t config = {
        .url = url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .keep_alive_enable = true,
        .timeout_ms = 60000,
        .buffer_size = 10240, // Increased for download speed
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &config,
    };

    esp_err_t ret = esp_https_ota(&ota_config);
    if (ret == ESP_OK) {
        ESP_LOGW(TAG_DB, "OTA Success! Rebooting...");
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    } else {
        ESP_LOGE(TAG_DB, "OTA Failed! Error: %d", ret);
    }

    // If it fails, clean up memory and delete this task
    free(url);
    vTaskDelete(NULL);
}
/**
 * WebSocket Event Handler: Handles Relay toggles AND OTA Updates
 */
static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    
    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG_DB, "WebSocket Connected. Joining Topics...");
            // Join Relays
            const char *jr = "{\"topic\":\"realtime:public:relays\",\"event\":\"phx_join\",\"payload\":{},\"ref\":\"1\"}";
            esp_websocket_client_send_text(ws_client, jr, strlen(jr), portMAX_DELAY);
            
            // Join System Control (OTA)
            const char *js = "{\"topic\":\"realtime:public:system_control\",\"event\":\"phx_join\",\"payload\":{},\"ref\":\"2\"}";
            esp_websocket_client_send_text(ws_client, js, strlen(js), portMAX_DELAY);
            break;

        case WEBSOCKET_EVENT_DATA:
            if (data->data_ptr != NULL && data->data_len > 0) {
                cJSON *root = cJSON_ParseWithLength(data->data_ptr, data->data_len);
                if (!root) break;

                cJSON *topic = cJSON_GetObjectItem(root, "topic");
                cJSON *event = cJSON_GetObjectItem(root, "event");
                cJSON *payload = cJSON_GetObjectItem(root, "payload");

                if (topic && event && payload) {
                    // --- ROUTE 1: RELAY UPDATES ---
                    if (strcmp(topic->valuestring, "realtime:public:relays") == 0 && strcmp(event->valuestring, "UPDATE") == 0) {
                        cJSON *record = cJSON_GetObjectItem(payload, "record");
                        if (record) {
                            int id = cJSON_GetObjectItem(record, "id")->valueint;
                            int state = cJSON_GetObjectItem(record, "state")->valueint;
                            if (id >= 1 && id <= 9) {
                                int idx = id - 1;
                                relay_states[idx] = state;
                                gpio_set_level(RELAY_GPIOS[idx], state);
                                ESP_LOGW(TAG_DB, "REALTIME RELAY: %d -> %d", id, state);
                                save_relay_states_to_nvs(); 
                            }
                        }
                    }
                    // --- ROUTE 2: OTA UPDATES ---
                    else if (strcmp(topic->valuestring, "realtime:public:system_control") == 0) {
                        cJSON *data_node = cJSON_GetObjectItem(payload, "record");
                        if (!data_node) data_node = cJSON_GetObjectItem(payload, "new");

                        if (data_node) {
                            cJSON *v_item = cJSON_GetObjectItem(data_node, "version");
                            cJSON *u_item = cJSON_GetObjectItem(data_node, "bin_url");

                            if (v_item && u_item) {
                                const char *new_ver = v_item->valuestring;
                                const char *raw_url = u_item->valuestring;

                                // URL Trimming logic
                                // char clean_url[512];
                                // strncpy(clean_url, raw_url, sizeof(clean_url) - 1);
                                // clean_url[sizeof(clean_url) - 1] = '\0';
                                // char *end = clean_url + strlen(clean_url) - 1;
                                // while(end > clean_url && (*end == ' ' || *end == '\n' || *end == '\r' || *end == '\t')) {
                                //     *end = '\0';
                                //     end--;
                                // }

                                ESP_LOGI(TAG_DB, "System Msg: Ver %s, Local: %s", new_ver, FIRMWARE_VERSION);

                                if (strcmp(new_ver, FIRMWARE_VERSION) != 0) {
                                    ESP_LOGW(TAG_DB, "!!! OTA TRIGGERED !!! Target: %s", new_ver);
                                    char *url_copy = strdup(raw_url);           
                                    xTaskCreate(ota_task, "ota_task", 12288, (void *)url_copy, 5, NULL);
                                    // start_ota_update(clean_url);
                                }
                            }
                        }
                    }
                }
                cJSON_Delete(root);
            }
            break;

        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGE(TAG_DB, "WebSocket Disconnected");
            break;
    }
}

void start_supabase_realtime(void) {
    char ws_url[1024];
    snprintf(ws_url, sizeof(ws_url), "wss://hbpwmqxwnnffgnncbgeq.supabase.co/realtime/v1/websocket?apikey=%s&vsn=1.0.0", SUPABASE_ANON_KEY);
    esp_websocket_client_config_t ws_cfg = { .uri = ws_url };
    ESP_LOGI(TAG_DB, "Connecting to Supabase Realtime...");
    ws_client = esp_websocket_client_init(&ws_cfg);
    esp_websocket_register_events(ws_client, WEBSOCKET_EVENT_ANY, websocket_event_handler, NULL);
    esp_websocket_client_start(ws_client);
}

void update_relay_state_in_supabase(int relay_id, int new_state) {
    if (esp_wifi_sta_get_ap_info(&(wifi_ap_record_t){0}) != ESP_OK) return;
    char url[256];
    snprintf(url, sizeof(url), "%s/rest/v1/%s?id=eq.%d", SUPABASE_URL, TABLE_RELAYS, relay_id);
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "state", new_state);
    char *json_string = cJSON_PrintUnformatted(root);
    esp_http_client_config_t config = { .url = url, .method = HTTP_METHOD_PATCH, .buffer_size = SB_HTTP_BUFFER_SIZE, .crt_bundle_attach = esp_crt_bundle_attach };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    char bearer[4096];
    snprintf(bearer, sizeof(bearer), "Bearer %s", SUPABASE_ANON_KEY);
    esp_http_client_set_header(client, "Authorization", bearer);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_string, strlen(json_string));
    esp_http_client_perform(client);
    esp_http_client_cleanup(client);
    cJSON_Delete(root); free(json_string);
}

void fetch_relay_states_from_supabase(void) {
    char url[256];
    snprintf(url, sizeof(url), "%s/rest/v1/%s?select=id,state&order=id.asc", SUPABASE_URL, TABLE_RELAYS);
    esp_http_client_config_t config = { .url = url, .method = HTTP_METHOD_GET, .buffer_size = SB_HTTP_BUFFER_SIZE, .crt_bundle_attach = esp_crt_bundle_attach };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    char bearer[4096];
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
                for (int i = 0; i < cJSON_GetArraySize(root) && i < 9; i++) {
                    cJSON *item = cJSON_GetArrayItem(root, i);
                    relay_states[i] = cJSON_GetObjectItem(item, "state")->valueint;
                    gpio_set_level(RELAY_GPIOS[i], relay_states[i]);
                }
            }
            cJSON_Delete(root);
        }
        free(buffer);
    }
    esp_http_client_cleanup(client);
}

void send_device_health_to_supabase(void) {
    char mac_str[18]; get_mac_address(mac_str);
    wifi_ap_record_t ap; esp_wifi_sta_get_ap_info(&ap);
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "device_id", mac_str);
    cJSON_AddNumberToObject(root, "wifi_rssi", ap.rssi);
    cJSON_AddNumberToObject(root, "uptime_seconds", esp_timer_get_time() / 1000000);
    cJSON_AddStringToObject(root, "firmware_version", FIRMWARE_VERSION);
    char *json_string = cJSON_PrintUnformatted(root);
    char url[256]; snprintf(url, sizeof(url), "%s/rest/v1/%s", SUPABASE_URL, TABLE_DEVICE_STATUS);
    esp_http_client_config_t config = { .url = url, .method = HTTP_METHOD_POST, .buffer_size = SB_HTTP_BUFFER_SIZE, .crt_bundle_attach = esp_crt_bundle_attach };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    char bearer[4096]; snprintf(bearer, sizeof(bearer), "Bearer %s", SUPABASE_ANON_KEY);
    esp_http_client_set_header(client, "Authorization", bearer);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Prefer", "resolution=merge-duplicates");
    esp_http_client_set_post_field(client, json_string, strlen(json_string));
    esp_http_client_perform(client);
    esp_http_client_cleanup(client);
    cJSON_Delete(root); free(json_string);
}

void supabase_sync_task(void *pvParameters) {
    while (esp_wifi_sta_get_ap_info(&(wifi_ap_record_t){0}) != ESP_OK) vTaskDelay(pdMS_TO_TICKS(1000));
    sync_all_relays_to_supabase();
    start_supabase_realtime();
    uint32_t last_health = 0, last_heartbeat = 0;
    while(1) {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (now - last_heartbeat >= 30000) {
            if (ws_client && esp_websocket_client_is_connected(ws_client)) {
                const char *hb = "{\"topic\":\"phoenix\",\"event\":\"heartbeat\",\"payload\":{},\"ref\":\"hb\"}";
                esp_websocket_client_send_text(ws_client, hb, strlen(hb), portMAX_DELAY);
            }
            last_heartbeat = now;
        }
        if (now - last_health >= 60000) {
            send_device_health_to_supabase();
            last_health = now;
        }
        vTaskDelay(pdMS_TO_TICKS(1000)); 
    }
}

void save_relay_states_to_nvs(void) {
    nvs_handle_t h;
    if (nvs_open(NVS_RELAY_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        for (int i = 0; i < 9; i++) {
            char key[4]; snprintf(key, sizeof(key), "r%d", i + 1);
            nvs_set_i32(h, key, relay_states[i]);
        }
        nvs_commit(h); nvs_close(h);
        ESP_LOGI(TAG_DB, "States Saved to Flash");
    }
}

void load_relay_states_from_nvs(void) {
    nvs_handle_t h;
    if (nvs_open(NVS_RELAY_NAMESPACE, NVS_READONLY, &h) != ESP_OK) {
        save_relay_states_to_nvs(); return;
    }
    for (int i = 0; i < 9; i++) {
        char key[4]; snprintf(key, sizeof(key), "r%d", i + 1);
        int32_t val = 0;
        if (nvs_get_i32(h, key, &val) == ESP_OK) {
            relay_states[i] = (int)val;
            gpio_set_level(RELAY_GPIOS[i], relay_states[i]);
        }
    }
    nvs_close(h);
    ESP_LOGW(TAG_DB, "Relays restored from Flash.");
}

void sync_all_relays_to_supabase(void) {
    cJSON *root = cJSON_CreateArray();
    for (int i = 0; i < 9; i++) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "id", i + 1);
        cJSON_AddNumberToObject(item, "state", relay_states[i]);
        cJSON_AddItemToArray(root, item);
    }
    char *js = cJSON_PrintUnformatted(root);
    char url[256]; snprintf(url, sizeof(url), "%s/rest/v1/%s", SUPABASE_URL, TABLE_RELAYS);
    esp_http_client_config_t cfg = { .url = url, .method = HTTP_METHOD_POST, .buffer_size = SB_HTTP_BUFFER_SIZE, .crt_bundle_attach = esp_crt_bundle_attach };
    esp_http_client_handle_t cli = esp_http_client_init(&cfg);
    char bearer[4096]; snprintf(bearer, sizeof(bearer), "Bearer %s", SUPABASE_ANON_KEY);
    esp_http_client_set_header(cli, "Authorization", bearer);
    esp_http_client_set_header(cli, "Content-Type", "application/json");
    esp_http_client_set_header(cli, "Prefer", "resolution=merge-duplicates");
    esp_http_client_set_post_field(cli, js, strlen(js));
    esp_http_client_perform(cli);
    esp_http_client_cleanup(cli);
    cJSON_Delete(root); free(js);
    ESP_LOGW(TAG_DB, "Cloud sync complete.");
}

void start_ota_update(const char *url) {
    ESP_LOGW(TAG_DB, "INITIALIZING OTA UPDATE...");
    esp_http_client_config_t cfg = { .url = url, .crt_bundle_attach = esp_crt_bundle_attach, .keep_alive_enable = true, .timeout_ms = 60000, .buffer_size = 8192 };
    esp_https_ota_config_t ota_cfg = { .http_config = &cfg };
    if (esp_https_ota(&ota_cfg) == ESP_OK) {
        ESP_LOGW(TAG_DB, "OTA Success! Rebooting...");
        vTaskDelay(pdMS_TO_TICKS(1000)); esp_restart();
    } else {
        ESP_LOGE(TAG_DB, "OTA Failed!");
    }
}