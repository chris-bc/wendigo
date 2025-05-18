#ifndef WENDIGO_COMMON_H
#define WENDIGO_COMMON_H

#include <esp_log.h>
#include <esp_err.h>

#include <esp_wifi.h>
#include <esp_wifi_types.h>

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
static const char STRING_MALLOC_FAIL[] = "Unable to allocate memory ";
static const char STRING_YES[] = "YES";
static const char STRING_NO[] = "NO";
static const char STRING_NA[] = "N/A";
static const char STRING_ACTIVE[] = "ACTIVE";
static const char STRING_IDLE[] = "IDLE";
static const char STRING_ENABLE[] = "ENABLE";
static const char STRING_DISABLE[] = "DISABLE";
static const char STRING_STATUS[] = "STATUS";

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
    SCAN_TAG,
    SCAN_FOCUS,
    SCAN_COUNT
} ScanType;

/* Globals for state management */
const char *TAG = "WENDIGO";
ActionType scanStatus[SCAN_COUNT] = { ACTION_DISABLE, ACTION_DISABLE, ACTION_DISABLE, ACTION_DISABLE, ACTION_DISABLE };
char *syntaxTip[SCAN_COUNT] = { "H[CI]", "B[LE]", "W[IFI]", "I[NTERACTIVE]", "T[AG] ( B[T] | W[IFI] ) <MAC>", "F[OCUS]" };
char *radioShortNames[SCAN_COUNT] = { "HCI", "BLE", "WiFi", "Interactive", "Tag", "Focus" };
char *radioFullNames[SCAN_COUNT] = { "Bluetooth Classic", "Bluetooth Low Energy", "WiFi", "Interactive Mode", "Tag Devices", "Focus Mode" };

/* Function declarations */
esp_err_t wendigo_bytes_to_string(uint8_t *bytes, char *string, int byteCount);
esp_err_t mac_bytes_to_string(uint8_t *bMac, char *strMac);
esp_err_t wendigo_string_to_bytes(char *strMac, uint8_t *bMac);
esp_err_t outOfMemory();

void print_star(int size, bool newline);
void print_space(int size, bool newline);
void repeat_bytes(uint8_t byte, uint8_t count);
void send_bytes(uint8_t *bytes, uint8_t size);

#endif
