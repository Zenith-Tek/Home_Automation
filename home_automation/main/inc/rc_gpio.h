#ifndef __RC_GPIO_H__
#define __RC_GPIO_H__

#include <stdint.h>
#include "esp_timer.h"  // for esp_timer_get_time()
// GPIO numbers
// *onb = On Board
// RELAY GPIO'S
#define RELAY_GPIO_ONB 13
#define RELAY_GPIO_1 14
#define RELAY_GPIO_2 4
#define RELAY_GPIO_3 16
#define RELAY_GPIO_4 17
#define RELAY_GPIO_5 5
#define RELAY_GPIO_6 18
#define RELAY_GPIO_7 19
#define RELAY_GPIO_8 21

// SWITCH GPIO'S
#define SWITCH_GPIO_1 36
#define SWITCH_GPIO_2 39
#define SWITCH_GPIO_3 34
#define SWITCH_GPIO_4 35
#define SWITCH_GPIO_5 32
#define SWITCH_GPIO_6 33
#define SWITCH_GPIO_7 25
#define SWITCH_GPIO_8 26
#define SWITCH_GPIO_ONB_1 22
#define SWITCH_GPIO_ONB_2 23

// Function to setup GPIOs (Relay output + Switch input with interrupt)
void setup_gpios(void);
void set_relay_gpios_out(void);
void set_switch_gpios_in(void);
#endif // __RC_GPIO_H__
