#ifndef WENDIGO_COMMON_H
#define WENDIGO_COMMON_H

/* Macro to mark a variable as unused in order to prevent compiler warnings
   This is defined in several of the bluetooth header files, but Bluetooth
   may not be present in the current chipset */
#ifndef UNUSED
    #define UNUSED(x) (void)(x)
#endif

#include <stdbool.h>

#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_interface.h>
#include <esp_wifi_types.h>

#include "wendigo.h"

/* Common string definitions */
char STRINGS_HOP_STATE_FAIL[] = "Unable to set hop state: ";
char STRINGS_MALLOC_FAIL[] = "Unable to allocate memory ";
char STRINGS_SET_MAC_FAIL[] = "Unable to set MAC ";
char STRINGS_HOPMODE_INVALID[] = "Invalid hopMode ";

typedef enum {
    MSG_ACK,
    MSG_OK,
    MSG_FAIL,
    MSG_ERROR,
    MSG_COUNT
} MsgType;

typedef enum {
    ACTION_DISABLE = 0,
    ACTION_ENABLE,
    ACTION_STATUS
} ActionType;

typedef enum {
    RADIO_HCI = 0,
    RADIO_BLE,
    RADIO_WIFI,
    RADIO_COUNT
} RadioType;

ActionType radioStatus[RADIO_COUNT] = { ACTION_DISABLE, ACTION_DISABLE, ACTION_DISABLE };

esp_err_t bytes_to_string(uint8_t *bytes, char *string, int byteCount);
esp_err_t mac_bytes_to_string(uint8_t *bMac, char *strMac);

#endif