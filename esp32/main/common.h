#ifndef WENDIGO_COMMON_H
#define WENDIGO_COMMON_H

#include <esp_log.h>
#include <esp_err.h>

#include <esp_wifi.h>
#include <esp_wifi_types.h>
#include <esp_gap_ble_api.h>
#include <esp_gatt_defs.h>
#include <esp_gattc_api.h>
#include <esp_gap_bt_api.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Macro to mark a variable as unused to prevent compiler warnings */
#ifndef UNUSED
    #define UNUSED(x) (void)(x)
#endif

#define MAX_SSID_LEN 32
#define MAC_STRLEN 17

/* Common string definitions */
char STRINGS_MALLOC_FAIL[] = "Unable to allocate memory ";

/* enums */
typedef enum {
    MSG_ACK,
    MSG_OK,
    MSG_FAIL,
    MSG_INVALID,
    MSG_COUNT
} MsgType;

typedef enum {
    ACTION_DISABLE = 0,
    ACTION_ENABLE,
    ACTION_STATUS,
    ACTION_INVALID
} ActionType;

typedef enum {
    SCAN_HCI = 0,
    SCAN_BLE,
    SCAN_WIFI,
    SCAN_INTERACTIVE,
    SCAN_COUNT
} ScanType;

/* Globals for state management */
const char *TAG = "WENDIGO";
ActionType scanStatus[SCAN_COUNT] = { ACTION_DISABLE, ACTION_DISABLE, ACTION_DISABLE, ACTION_DISABLE };
char *syntaxTip[SCAN_COUNT] = { "H[CI]", "B[LE]", "W[IFI]", "I[NTERACTIVE]" };

/* Function declarations */
esp_err_t bytes_to_string(uint8_t *bytes, char *string, int byteCount);
esp_err_t mac_bytes_to_string(uint8_t *bMac, char *strMac);
esp_err_t mac_string_to_bytes(char *strMac, uint8_t *bMac);

#endif
