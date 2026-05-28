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
#include "wifi_provisioning/manager.h"

#define TAG_DB "Supabase"

// Global and Static Variables
int relay_states[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
const int RELAY_GPIOS[9] = {13, 14, 4, 16, 17, 5, 18, 19, 21};
static esp_websocket_client_handle_t ws_client = NULL;

// Forward Declarations
static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
static void set_supabase_auth_headers(esp_http_client_handle_t client);

// Task wrapper to run reprovision with sufficient stack
static void reprovision_task(void *pvParameters) {
    trigger_software_reprovision();
    vTaskDelete(NULL);
}

/**
 * Helper: Get MAC Address
 */
void get_mac_address(char *mac_str) {
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    sprintf(mac_str, "%02X:%02X:%02X:%02X:%02X:%02X", 
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

/**
 * Helper: Set standard Headers safely using stack buffer
 */
static void set_supabase_auth_headers(esp_http_client_handle_t client) {
    esp_http_client_set_header(client, "apikey", SUPABASE_ANON_KEY);
    // Use a stack buffer large enough for "Bearer " + key
    char bearer[512];
    snprintf(bearer, sizeof(bearer), "Bearer %s", SUPABASE_ANON_KEY);
    // esp_http_client_set_header copies the value internally, so stack is safe
    esp_http_client_set_header(client, "Authorization", bearer);
    esp_http_client_set_header(client, "Content-Type", "application/json");
}

/**
 * @brief Updates the state of a specific relay in the Supabase database.
 * @param relay_id The database ID of the relay (1-9)
 * @param new_state 0 for OFF, 1 for ON
 */
void update_relay_state_in_supabase(int relay_id, int new_state) {
    // Skip cloud sync if WiFi is not connected yet
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK) {
        ESP_LOGW(TAG_DB, "WiFi not connected, skipping Supabase sync for relay %d", relay_id);
        return;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "id", relay_id);
    cJSON_AddNumberToObject(root, "state", new_state);
    char *json_string = cJSON_PrintUnformatted(root);

    char url[256];
    snprintf(url, sizeof(url), "%s/rest/v1/%s", SUPABASE_URL, TABLE_RELAYS);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .buffer_size = SB_HTTP_BUFFER_SIZE,
        .buffer_size_tx = SB_HTTP_TX_BUFFER_SIZE,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    set_supabase_auth_headers(client);
    esp_http_client_set_header(client, "Prefer", "resolution=merge-duplicates");
    esp_http_client_set_post_field(client, json_string, strlen(json_string));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG_DB, "Relay %d state updated to %d in Supabase", relay_id, new_state);
    } else {
        ESP_LOGE(TAG_DB, "Failed to update relay %d: %s", relay_id, esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    cJSON_Delete(root);
    free(json_string);
}

/**
 * @brief Fetches all relay states from Supabase and applies them to GPIOs.
 */
void fetch_relay_states_from_supabase(void) {
    char url[256];
    snprintf(url, sizeof(url), "%s/rest/v1/%s?select=id,state&order=id.asc", SUPABASE_URL, TABLE_RELAYS);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .buffer_size = SB_HTTP_BUFFER_SIZE,
        .buffer_size_tx = SB_HTTP_TX_BUFFER_SIZE,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    set_supabase_auth_headers(client);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int content_length = esp_http_client_get_content_length(client);
        if (content_length > 0 && content_length < SB_HTTP_BUFFER_SIZE) {
            char *buffer = malloc(content_length + 1);
            if (buffer) {
                esp_http_client_read(client, buffer, content_length);
                buffer[content_length] = '\0';

                cJSON *arr = cJSON_Parse(buffer);
                if (arr && cJSON_IsArray(arr)) {
                    int size = cJSON_GetArraySize(arr);
                    for (int i = 0; i < size && i < 9; i++) {
                        cJSON *item = cJSON_GetArrayItem(arr, i);
                        cJSON *id_item = cJSON_GetObjectItem(item, "id");
                        cJSON *state_item = cJSON_GetObjectItem(item, "state");
                        if (id_item && state_item) {
                            int id = id_item->valueint;
                            int state = state_item->valueint;
                            if (id >= 1 && id <= 9) {
                                relay_states[id - 1] = state;
                                gpio_set_level(RELAY_GPIOS[id - 1], state);
                            }
                        }
                    }
                    ESP_LOGI(TAG_DB, "Fetched relay states from Supabase");
                }
                cJSON_Delete(arr);
                free(buffer);
            }
        }
    } else {
        ESP_LOGE(TAG_DB, "Failed to fetch relay states: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

/**
 * @brief Starts an OTA update from the given URL.
 */
void start_ota_update(const char *url) {
    if (!url) return;
    char *url_copy = strdup(url);
    if (url_copy) {
        xTaskCreate(ota_task, "ota_task", 12288, (void *)url_copy, 10, NULL);
    }
}

/**
 * OTA Background Task
 */
void ota_task(void *pvParameter) {
    char *url = (char *)pvParameter;
    ESP_LOGW(TAG_DB, "OTA TASK: Downloading firmware from [%s]", url);

    esp_http_client_config_t config = {
        .url = url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .keep_alive_enable = true,
        .timeout_ms = 60000,
        .buffer_size = 10240, 
    };

    esp_https_ota_config_t ota_config = { .http_config = &config };
    esp_err_t ret = esp_https_ota(&ota_config);
    
    if (ret == ESP_OK) {
        ESP_LOGW(TAG_DB, "OTA Success! Rebooting...");
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    } else {
        ESP_LOGE(TAG_DB, "OTA Failed! Error: %d", ret);
    }
    free(url);
    vTaskDelete(NULL);
}

/**
 * WebSocket Event Handler
 */
static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    
    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG_DB, "WebSocket Connected. Joining Topics...");
            const char *jr = "{\"topic\":\"realtime:public:relays\",\"event\":\"phx_join\",\"payload\":{},\"ref\":\"1\"}";
            esp_websocket_client_send_text(ws_client, jr, strlen(jr), portMAX_DELAY);
            
            const char *js = "{\"topic\":\"realtime:public:system_control\",\"event\":\"phx_join\",\"payload\":{},\"ref\":\"2\"}";
            esp_websocket_client_send_text(ws_client, js, strlen(js), portMAX_DELAY);

            const char *jd = "{\"topic\":\"realtime:public:device_status\",\"event\":\"phx_join\",\"payload\":{\"config\":{\"postgres_changes\":[{\"event\":\"UPDATE\",\"schema\":\"public\",\"table\":\"device_status\"}]}},\"ref\":\"3\"}";
            esp_websocket_client_send_text(ws_client, jd, strlen(jd), portMAX_DELAY);
            break;

        case WEBSOCKET_EVENT_DATA:
            // Skip fragmented messages - only process when we have the complete payload
            if (data->payload_offset != 0 || data->data_len != data->payload_len) {
                ESP_LOGW(TAG_DB, "WS fragmented msg: offset=%d, data_len=%d, payload_len=%d (skipped)",
                         data->payload_offset, data->data_len, data->payload_len);
                break;
            }
            if (data->data_ptr != NULL && data->data_len > 0) {
                cJSON *root = cJSON_ParseWithLength(data->data_ptr, data->data_len);
                if (!root) break;

                cJSON *topic = cJSON_GetObjectItem(root, "topic");
                cJSON *event = cJSON_GetObjectItem(root, "event");
                cJSON *payload = cJSON_GetObjectItem(root, "payload");

                if (topic && event && payload && cJSON_IsString(topic)) {
                    const char* topic_str = topic->valuestring;

                    // --- 1. RELAYS ---
                    if (strcmp(topic_str, "realtime:public:relays") == 0) {
                        cJSON *record = cJSON_GetObjectItem(payload, "record");
                        if (record) {
                            cJSON *id_item = cJSON_GetObjectItem(record, "id");
                            cJSON *st_item = cJSON_GetObjectItem(record, "state");
                            if (id_item && st_item) {
                                int id = id_item->valueint;
                                int st = st_item->valueint;
                                if (id >= 1 && id <= 9) {
                                    relay_states[id-1] = st;
                                    gpio_set_level(RELAY_GPIOS[id-1], st);
                                    save_relay_states_to_nvs();
                                    ESP_LOGW(TAG_DB, "REALTIME RELAY: %d -> %d", id, st);
                                }
                            }
                        }
                    }
                    // --- 2. OTA (SYSTEM_CONTROL) ---
                    else if (strcmp(topic_str, "realtime:public:system_control") == 0) {
                        cJSON *data_node = cJSON_GetObjectItem(payload, "record");
                        if (!data_node) data_node = cJSON_GetObjectItem(payload, "new");
                        if (data_node) {
                            cJSON *v_item = cJSON_GetObjectItem(data_node, "version");
                            cJSON *u_item = cJSON_GetObjectItem(data_node, "bin_url");
                            if (v_item && cJSON_IsString(v_item) && u_item && cJSON_IsString(u_item)) {
                                if (strcmp(v_item->valuestring, FIRMWARE_VERSION) != 0) {
                                    // Trim URL logic
                                    char *clean_url = strdup(u_item->valuestring);
                                    if (clean_url) {
                                        char *end = clean_url + strlen(clean_url) - 1;
                                        while(end > clean_url && (*end == ' ' || *end == '\n' || *end == '\r')) { *end = '\0'; end--; }
                                        
                                        ESP_LOGW(TAG_DB, "OTA TRIGGERED! Target: %s", clean_url);
                                        xTaskCreate(ota_task, "ota_task", 12288, (void *)clean_url, 10, NULL);
                                    }
                                }
                            }
                        }
                    }
                    // --- 3. REPROVISION (DEVICE_STATUS) ---
                    else if (strcmp(topic_str, "realtime:public:device_status") == 0) {
                        const char *evt_str = cJSON_IsString(event) ? event->valuestring : "?";
                        ESP_LOGW(TAG_DB, "DEVICE_STATUS event: %s", evt_str);
                        char *dbg = cJSON_PrintUnformatted(payload);
                        if (dbg) { ESP_LOGW(TAG_DB, "DEVICE_STATUS payload: %s", dbg); free(dbg); }

                        // With postgres_changes config, data arrives under payload.data.record
                        cJSON *data_wrapper = cJSON_GetObjectItem(payload, "data");
                        cJSON *data_node = NULL;
                        if (data_wrapper) {
                            data_node = cJSON_GetObjectItem(data_wrapper, "record");
                        }
                        // Fallback for legacy format
                        if (!data_node) data_node = cJSON_GetObjectItem(payload, "record");
                        if (!data_node) data_node = cJSON_GetObjectItem(payload, "new");
                        if (data_node) {
                            cJSON *dev_id = cJSON_GetObjectItem(data_node, "device_id");
                            cJSON *trig = cJSON_GetObjectItem(data_node, "reprovision_trigger");

                            ESP_LOGW(TAG_DB, "REPROV CHECK: dev_id=%s, trig_type=%d, trig_val=%d",
                                     (dev_id && cJSON_IsString(dev_id)) ? dev_id->valuestring : "null",
                                     trig ? trig->type : -1,
                                     trig ? trig->valueint : -999);

                            if (dev_id && cJSON_IsString(dev_id) && trig && trig->valueint == 0) {
                                char my_mac[18]; get_mac_address(my_mac);
                                ESP_LOGW(TAG_DB, "MAC compare: theirs=%s ours=%s", dev_id->valuestring, my_mac);
                                if (strcasecmp(dev_id->valuestring, my_mac) == 0) {
                                    ESP_LOGE(TAG_DB, "SOFTWARE REPROVISION TRIGGERED!");
                                    xTaskCreate(reprovision_task, "reprov_task", 8192, NULL, 5, NULL);
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
    esp_websocket_client_config_t ws_cfg = {
        .uri = ws_url,
        .buffer_size = 4096,
    };
    ws_client = esp_websocket_client_init(&ws_cfg);
    esp_websocket_register_events(ws_client, WEBSOCKET_EVENT_ANY, websocket_event_handler, NULL);
    esp_websocket_client_start(ws_client);
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
    esp_http_client_config_t config = { .url = url, .method = HTTP_METHOD_POST, .buffer_size = SB_HTTP_BUFFER_SIZE, .buffer_size_tx = SB_HTTP_TX_BUFFER_SIZE, .crt_bundle_attach = esp_crt_bundle_attach };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    set_supabase_auth_headers(client);
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
        ESP_LOGI(TAG_DB, "Relay states saved to Flash");
    }
}

void load_relay_states_from_nvs(void) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_RELAY_NAMESPACE, NVS_READONLY, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) { save_relay_states_to_nvs(); return; }
    if (err != ESP_OK) return;
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
    esp_http_client_config_t cfg = { .url = url, .method = HTTP_METHOD_POST, .buffer_size = SB_HTTP_BUFFER_SIZE, .buffer_size_tx = SB_HTTP_TX_BUFFER_SIZE, .crt_bundle_attach = esp_crt_bundle_attach };
    esp_http_client_handle_t cli = esp_http_client_init(&cfg);
    set_supabase_auth_headers(cli);
    esp_http_client_set_header(cli, "Prefer", "resolution=merge-duplicates");
    esp_http_client_set_post_field(cli, js, strlen(js));
    esp_http_client_perform(cli);
    esp_http_client_cleanup(cli);
    cJSON_Delete(root); free(js);
    ESP_LOGW(TAG_DB, "Cloud sync complete.");
}

void trigger_software_reprovision(void) {
    save_relay_states_to_nvs();
    sync_all_relays_to_supabase();
    // Final heartbeat to reset trigger
    char mac[18]; get_mac_address(mac);
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "device_id", mac);
    cJSON_AddNumberToObject(root, "reprovision_trigger", 1);
    char *js = cJSON_PrintUnformatted(root);
    char url[256]; snprintf(url, sizeof(url), "%s/rest/v1/%s", SUPABASE_URL, TABLE_DEVICE_STATUS);
    esp_http_client_config_t cfg = { .url = url, .method = HTTP_METHOD_POST, .buffer_size = SB_HTTP_BUFFER_SIZE, .buffer_size_tx = SB_HTTP_TX_BUFFER_SIZE, .crt_bundle_attach = esp_crt_bundle_attach };
    esp_http_client_handle_t cli = esp_http_client_init(&cfg);
    set_supabase_auth_headers(cli);
    esp_http_client_set_header(cli, "Prefer", "resolution=merge-duplicates");
    esp_http_client_set_post_field(cli, js, strlen(js));
    esp_http_client_perform(cli);
    esp_http_client_cleanup(cli);
    cJSON_Delete(root); free(js);

    ESP_LOGW(TAG_DB, "Erasing WiFi and restarting...");
    wifi_prov_mgr_reset_provisioning();
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
}
