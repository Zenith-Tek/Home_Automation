#ifndef __SUPABASE_H__
#define __SUPABASE_H__

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "cJSON.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "esp_http_client.h"
#include "esp_mac.h" 

#include "nvs_flash.h"
#include "nvs.h"

/* ==================================================================== */
/* ==================== CONFIGURATION ================================= */
/* ==================================================================== */
#define SB_HTTP_BUFFER_SIZE 16384

#define NVS_NAMESPACE "relay_storage"
#define NVS_KEY_STATES "states"
#define NVS_RELAY_NAMESPACE "relays_pref"

// --- Supabase Configuration ---
// These values are taken directly from your Supabase project settings.
#define SUPABASE_URL "https://hbpwmqxwnnffgnncbgeq.supabase.co"
#define SUPABASE_ANON_KEY "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6ImhicHdtcXh3bm5mZmdubmNiZ2VxIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NzkyMDE0NDEsImV4cCI6MjA5NDc3NzQ0MX0.XcNGCIHBePe0aZRwYwKJSJTO4W-FjAdINeRjDB9XRE4"

// Table Names
#define TABLE_RELAYS "relays"
#define TABLE_DEVICE_STATUS "device_status"

/* ==================== GLOBALS / STATE =============================== */
// Array to hold the current state of 9 relays (0 = OFF, 1 = ON)
extern int relay_states[9]; 

/* ==================== FUNCTIONS ===================================== */

/**
 * @brief Fetches all relay states from Supabase. 
 * Use this on boot-up for state retention.
 */
void fetch_relay_states_from_supabase(void);

/**
 * @brief Updates the state of a specific relay in the database.
 * @param relay_id The database ID of the relay (1-9)
 * @param new_state 0 for OFF, 1 for ON
 */
void update_relay_state_in_supabase(int relay_id, int new_state);

/**
 * @brief Sends heartbeats to the 'device_status' table.
 * Includes RSSI and uptime for monitoring.
 */
void send_device_health_to_supabase(void);

/**
 * @brief Background task to periodically sync status or check for remote commands.
 */
void supabase_sync_task(void *pvParameters);

// New function for Realtime
void start_supabase_realtime(void);

void save_relay_states_to_nvs(void);
void load_relay_states_from_nvs(void);
void sync_all_relays_to_supabase(void);

#endif // __SUPABASE_H__