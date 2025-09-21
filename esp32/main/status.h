#ifndef WENDIGO_STATUS_H
#define WENDIGO_STATUS_H

#include <stdbool.h>

void display_status_interactive();
void display_status_uart();

/* Status attributes representing interface support (e.g. Bluetooth Classic
 * support, WiFi support) are also used to determine which features to
 * enable/disable, so let's #define them. */
#define STRING_BT_UUID_DICTIONARY               "BT UUID dictionary?"
#define STRING_BT_CLASSIC_SUPPORTED             "BT Classic Support?"
#define STRING_BLE_SUPPORTED                    "BT Low Energy Support?"
#define STRING_WIFI_24_SUPPORTED                "WiFi 2.4GHz Support?"
#define STRING_WIFI_5_SUPPORTED                 "WiFi 5GHz Support?"
#define STRING_BT_CLASSIC_COUNT                 "BT Classic Devices:"
#define STRING_BLE_COUNT                        "BT Low Energy Devices:"
#define STRING_WIFI_STA_COUNT                   "WiFi STA Devices:"
#define STRING_WIFI_AP_COUNT                    "WiFi APs:"
#define STRING_BT_CLASSIC_SCANNING              "BT Classic Scanning:"
#define STRING_BLE_SCANNING                     "BT Low Energy Scanning:"
#define STRING_WIFI_SCANNING                    "WiFi Scanning:"
static const char STRING_YES[] =                "YES";
static const char STRING_NO[] =                 "NO";


#endif