#pragma once
#include <Arduino.h>


extern EventGroupHandle_t stateEventGroupHandle;

// event bits
#define STATE_SENSORS_READY (1<<0)
#define STATE_SENSORS_READ_EVENT (1<<1)
#define STATE_BUCK_MODE_CIV (1<<2)
#define STATE_BUCK_MODE_COV (1<<3)
#define STATE_BUCK_MODE_COC (1<<4)
#define STATE_BUCK_MODE_SAFED (1<<5)
#define STATE_BACKFLOW_ENABLED (1<<6)
#define STATE_BUCK_ENABLED (1<<7)
#define STATE_NETWORK_CONNECTED (1<<8)
/*#define ERROR_ (1<<9)
#define ERROR_ (1<<10)
#define ERROR_ (1<<11)
#define ERROR_ (1<<12)
#define ERROR_ (1<<13)
#define ERROR_ (1<<14)
#define ERROR_ (1<<15)
*/
#define STATE_BIT_NAMES "\"STATE_SENSORS_READY\", \"STATE_SENSORS_READ_EVENT\", \
\"STATE_BUCK_MODE_CIV\", \"STATE_BUCK_MODE_COV\", \"STATE_BUCK_MODE_COC\", \
\"STATE_BUCK_MODE_SAFED\", \"STATE_BACKFLOW_ENABLED\", \"STATE_NETWORK_CONNECTED\""

