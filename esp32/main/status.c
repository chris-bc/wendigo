#include "status.h"
#include "common.h"

#define NAME_MAX_LEN   (uint8_t)35
#define VAL_MAX_LEN    (uint8_t)20
#define ATTR_COUNT_MAX (uint8_t)13

char *attribute_names[] = {"Version:", "Chris Bennetts-Cash", "BT UUID Dictionary?", "BT Classic Support?",
                           "BT Low Energy Support?", "WiFi Support?", "BT Classic Scanning:",
                           "BT Low Energy Scanning:", "WiFi Scanning:", "BT Classic Devices:",
                           "BT Low Energy Devices:", "WiFi STA Devices:", "WiFi APs:"};
char attribute_values[ATTR_COUNT_MAX][VAL_MAX_LEN];

uint16_t classicDeviceCount = 0;
uint16_t leDeviceCount = 0;
uint16_t wifiSTACount = 0;
uint16_t wifiAPCount = 0;

enum StatusAttributes {
    ATTR_VERSION = 0,
    ATTR_GITHUB,
    ATTR_UUID_DICTIONARY,
    ATTR_BT_CLASSIC_SUPPORT,
    ATTR_BT_BLE_SUPPORT,
    ATTR_WIFI_SUPPORT,
    ATTR_BT_CLASSIC_SCANNING,
    ATTR_BT_BLE_SCANNING,
    ATTR_WIFI_SCANNING,
    ATTR_BT_CLASSIC_COUNT,
    ATTR_BT_BLE_COUNT,
    ATTR_WIFI_STA_COUNT,
    ATTR_WIFI_AP_COUNT,
};

void print_status_row_start(int spaces) {
    print_star(1, false);
    print_space(spaces, false);
}
void print_status_row_end(int spaces) {
    print_space(spaces, false);
    print_star(1, true);
}
void print_empty_row(int lineLength) {
    print_star(1, false);
    print_space(lineLength - 2, false);
    print_star(1, true);
}

/** Prepares data for display by the status command.
 * The function populates attribute_values[].
 */
void initialise_status_details(bool uuidDictionarySupported, bool btClassicSupported,
                                bool btBLESupported, bool wifiSupported) {
    const char github[] = "github.com/chris-bc";

    /* strncpy doesn't always add the terminating null byte, so start with a sequence of null bytes */
    for (uint8_t i = 0; i < ATTR_COUNT_MAX; ++i) {
        explicit_bzero(attribute_values[i], VAL_MAX_LEN);
    }

    strncpy(attribute_values[ATTR_VERSION], WENDIGO_VERSION, strlen(WENDIGO_VERSION) + 1);
    strncpy(attribute_values[ATTR_GITHUB], github, strlen(github));
    strncpy(attribute_values[ATTR_UUID_DICTIONARY], (uuidDictionarySupported) ? STRING_YES : STRING_NO, VAL_MAX_LEN);
    strncpy(attribute_values[ATTR_BT_CLASSIC_SUPPORT], (btClassicSupported) ? STRING_YES : STRING_NO, VAL_MAX_LEN);
    strncpy(attribute_values[ATTR_BT_BLE_SUPPORT], (btBLESupported) ? STRING_YES : STRING_NO, VAL_MAX_LEN);
    strncpy(attribute_values[ATTR_WIFI_SUPPORT], (wifiSupported) ? STRING_YES : STRING_NO, VAL_MAX_LEN);
    strncpy(attribute_values[ATTR_BT_CLASSIC_SCANNING], (scanStatus[SCAN_HCI] == ACTION_ENABLE) ? STRING_ACTIVE : STRING_IDLE, VAL_MAX_LEN);
    strncpy(attribute_values[ATTR_BT_BLE_SCANNING], (scanStatus[SCAN_BLE] == ACTION_ENABLE) ? STRING_ACTIVE : STRING_IDLE, VAL_MAX_LEN);
    strncpy(attribute_values[ATTR_WIFI_SCANNING], (scanStatus[SCAN_WIFI_AP] == ACTION_ENABLE || scanStatus[SCAN_WIFI_STA] == ACTION_ENABLE) ? STRING_ACTIVE : STRING_IDLE, VAL_MAX_LEN);
    /* Create device counts for BT Classic, BLE, AP and STA */
    classicDeviceCount = 0;
    leDeviceCount = 0;
    wifiSTACount = 0;
    wifiAPCount = 0;
    for (uint16_t i = 0; i < devices_count; ++i) {
        if (devices[i].scanType == SCAN_HCI) {
            ++classicDeviceCount;
        } else if (devices[i].scanType == SCAN_BLE) {
            ++leDeviceCount;
        } else if (devices[i].scanType == SCAN_WIFI_AP) {
            ++wifiAPCount;
        } else if (devices[i].scanType == SCAN_WIFI_STA) {
            ++wifiSTACount;
        } else {
            /* No action required */
        }
    }

    /* Need to stringify device counts */
    snprintf(attribute_values[ATTR_BT_CLASSIC_COUNT], VAL_MAX_LEN, "%d", classicDeviceCount);
    snprintf(attribute_values[ATTR_BT_BLE_COUNT], VAL_MAX_LEN, "%d", leDeviceCount);
    snprintf(attribute_values[ATTR_WIFI_STA_COUNT], VAL_MAX_LEN, "%d", wifiSTACount);
    snprintf(attribute_values[ATTR_WIFI_AP_COUNT], VAL_MAX_LEN, "%d", wifiAPCount);

    /* Now values have been written, loop through attributes again to ensure everything has a null byte */
    for (uint8_t i = 0; i < ATTR_COUNT_MAX; ++i) {
        attribute_values[i][VAL_MAX_LEN - 1] = '\0';
    }
}

void display_status_interactive(bool uuidDictionarySupported, bool btClassicSupported,
                                bool btBLESupported, bool wifiSupported) {
    const char *uuidDictionary = (uuidDictionarySupported) ? STRING_YES : STRING_NO;
    const char *btClassicSupport = (btClassicSupported) ? STRING_YES : STRING_NO;
    const char *btBLESupport = (btBLESupported) ? STRING_YES : STRING_NO;
    const char *wifiSupport = (wifiSupported) ? STRING_YES : STRING_NO;
    /* While we're not using attribute_names[] and attribute_values[], this
       function also generates device counts for different device types. */
    initialise_status_details(uuidDictionarySupported, btClassicSupported, btBLESupported, wifiSupported);

    print_star(53, true);
    print_empty_row(53);
    print_status_row_start(14);
    printf("Wendigo version %7s", WENDIGO_VERSION);
    print_status_row_end(14);
    print_empty_row(53);
    print_status_row_start(5);
    printf("Chris Bennetts-Cash   github.com/chris-bc");
    print_status_row_end(5);
    print_empty_row(53);
    print_empty_row(53);
    print_status_row_start(4);
    printf("Compiled with Bluetooth Dictionary: %7s", uuidDictionary);
    print_status_row_end(4);
    print_status_row_start(4);
    printf("Bluetooth Classic Supported: %14s", btClassicSupport);
    print_status_row_end(4);
    print_status_row_start(4);
    printf("Bluetooth Low Energy Supported: %11s", btBLESupport);
    print_status_row_end(4);
    print_status_row_start(4);
    printf("WiFi Supported: %27s", wifiSupport);
    print_status_row_end(4);
    print_status_row_start(4);
    printf("Bluetooth Classic Scanning: %15s", (!btClassicSupported) ? STRING_NA : (scanStatus[SCAN_HCI] == ACTION_ENABLE) ? STRING_ACTIVE : STRING_IDLE);
    print_status_row_end(4);
    print_status_row_start(4);
    printf("Bluetooth Low Energy Scanning: %12s", (!btBLESupported) ? STRING_NA : (scanStatus[SCAN_BLE] == ACTION_ENABLE) ? STRING_ACTIVE : STRING_IDLE);
    print_status_row_end(4);
    print_status_row_start(4);
    printf("WiFi Scanning: %28s", (!wifiSupported) ? STRING_NA : (scanStatus[SCAN_WIFI_AP] == ACTION_ENABLE || scanStatus[SCAN_WIFI_STA] == ACTION_ENABLE) ? STRING_ACTIVE : STRING_IDLE);
    print_status_row_end(4);
    print_status_row_start(4); // 43 total
    printf("BT Classic Devices: %23d", classicDeviceCount);
    print_status_row_end(4);
    print_status_row_start(4);
    printf("BT Low Energy Devices: %20d", leDeviceCount);
    print_status_row_end(4);
    print_status_row_start(4);
    printf("WiFi Access Points: %23d", wifiAPCount);
    print_status_row_end(4);
    print_status_row_start(4);
    printf("WiFi Stations: %28d", wifiSTACount);
    print_status_row_end(4);
    print_empty_row(53);
    print_star(53, true);
    // TODO: Device counts
}

/** Send status information to Flipper Zero.
 *  A status packet commences with 4 bytes of 0xEE followed by 4 bytes of 0xBB
 *  This is followed by 1 byte that specifies the number of attributes contained
 *  in the packet and then repeats the following pattern that number of tumes:
 *  * 1 byte specifying the length of the attribute name
 *  * The attribute name (the terminating'\0' is ommitted)
 *  * 1 byte specifying the legth of the attribute value
 *  * The attribute value (the terminating '\0' is ommitted)
 *  The packet is terminated with 4 bytes of 0xAA and 4 bytes of 0xFF.
 */
void display_status_uart(bool uuidDictionarySupported, bool btClassicSupported,
                                bool btBLESupported, bool wifiSupported) {
    initialise_status_details(uuidDictionarySupported, btClassicSupported, btBLESupported, wifiSupported);

    wendigo_get_tx_lock(true); /* Wait for the talking stick */
    send_bytes(PREAMBLE_STATUS, PREAMBLE_LEN);

    uint8_t attr_count = ATTR_COUNT_MAX;
    send_bytes(&attr_count, 1);

    /* Loop ATTR_COUNT_MAX times, sending elements from attribute_names[] and attribute_values[] */
    uint8_t len;
    for (uint8_t i = 0; i < ATTR_COUNT_MAX; ++i) {
        len = strlen(attribute_names[i]);
        send_bytes(&len, 1);
        send_bytes((uint8_t *)(attribute_names[i]), len);
        len = strlen(attribute_values[i]);
        send_bytes(&len, 1);
        send_bytes((uint8_t *)(attribute_values[i]), len);
    }
    send_end_of_packet();
    wendigo_release_tx_lock();
}
