#include <stdbool.h>
#include <stdint.h>
#include <sys/time.h>

/* We can determine whether it's the ESP32 app by checking for an ESP-IDF target device */
#if defined(CONFIG_IDF_TARGET)
    #include <esp_bt_defs.h>
    #include <esp_wifi_types.h>
#else
    #include <gui/modules/variable_item_list.h>
    #define IS_FLIPPER_APP      (1)
#endif

#include "wendigo_common_defs.h"

/* Packet identifiers */
uint8_t PREAMBLE_LEN	    = 8;
uint8_t PREAMBLE_BT_BLE[]   = {0xFF, 0xFF, 0xFF, 0xFF, 0xAA, 0xAA, 0xAA, 0xAA};
uint8_t PREAMBLE_WIFI_AP[]  = {0x99, 0x99, 0x99, 0x99, 0x11, 0x11, 0x11, 0x11};
uint8_t PREAMBLE_WIFI_STA[] = {0x99, 0x99, 0x99, 0x99, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t PREAMBLE_CHANNELS[] = {0x99, 0x99, 0x99, 0x99, 0xAA, 0xAA, 0xAA, 0xAA};
uint8_t PREAMBLE_STATUS[]   = {0xEE, 0xEE, 0xEE, 0xEE, 0xBB, 0xBB, 0xBB, 0xBB};
uint8_t PREAMBLE_VER[]      = {'W', 'e', 'n', 'd', 'i', 'g', 'o', ' '};
uint8_t PACKET_TERM[]       = {0xAA, 0xAA, 0xAA, 0xAA, 0xFF, 0xFF, 0xFF, 0xFF};

uint8_t nullMac[]           = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint8_t broadcastMac[]	    = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

/* enum ScanType being replaced with uint8_t */
const uint8_t SCAN_HCI          = 0;
const uint8_t SCAN_BLE          = 1;
const uint8_t SCAN_WIFI_AP      = 2;
const uint8_t SCAN_WIFI_STA     = 3;
const uint8_t SCAN_INTERACTIVE  = 4;
const uint8_t SCAN_TAG          = 5;
const uint8_t SCAN_FOCUS        = 6;
const uint8_t SCAN_COUNT        = 7;