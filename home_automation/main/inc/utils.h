#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdint.h>
#include <esp_log.h>

extern uint32_t tick;

// --- Function declarations ---
void process_gpios(int triggered_io);
void relay_on(int relay_id); // Updated to accept ID (1-9)
void relay_off(int relay_id); // Updated to accept ID (1-9)
void control_relay(int relay_id, int state);

#endif // __UTILS_H__