#include "status.h"
#include "common.h"

#define NAME_MAX_LEN   (uint8_t)35
#define VAL_MAX_LEN    (uint8_t)8
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

/** Free the initialised details when we're done with them  */
void free_status_details() {
    /* Actually there's nothing to do here, I think */
}

/** Prepares data for display by the status command.
 * The function populates attribute_values[].
 */
void initialise_status_details(bool uuidDictionarySupported, bool btClassicSupported,
                                bool btBLESupported, bool wifiSupported) {
    const char github[] = "github.com/chris-bc/wendigo";

    strncpy(attribute_values[0], WENDIGO_VERSION, strlen(WENDIGO_VERSION));
    strncpy(attribute_values[1], github, strlen(github));
    strncpy(attribute_values[2], (uuidDictionarySupported) ? STRING_YES : STRING_NO, VAL_MAX_LEN);
    strncpy(attribute_values[3], (btClassicSupported) ? STRING_YES : STRING_NO, VAL_MAX_LEN);
    strncpy(attribute_values[4], (btBLESupported) ? STRING_YES : STRING_NO, VAL_MAX_LEN);
    strncpy(attribute_values[5], (wifiSupported) ? STRING_YES : STRING_NO, VAL_MAX_LEN);
    strncpy(attribute_values[6], (scanStatus[SCAN_HCI] == ACTION_ENABLE) ? STRING_ACTIVE : STRING_IDLE, VAL_MAX_LEN);
    strncpy(attribute_values[7], (scanStatus[SCAN_BLE] == ACTION_ENABLE) ? STRING_ACTIVE : STRING_IDLE, VAL_MAX_LEN);
    strncpy(attribute_values[8], (scanStatus[SCAN_WIFI] == ACTION_ENABLE) ? STRING_ACTIVE : STRING_IDLE, VAL_MAX_LEN);
    /* num_gap_devices counts both Classic and LE devices. Create separate counts instead */
    classicDeviceCount = 0;
    leDeviceCount = 0;
    for (uint16_t i = 0; i < num_gap_devices; ++i) {
        switch (all_gap_devices[i].scanType) {
            case SCAN_HCI:
                ++classicDeviceCount;
                break;
            case SCAN_BLE:
                ++leDeviceCount;
                break;
            default:
                /* No action required */
                break;
        }
    }
    // TODO: WiFi stuff to set wifiSTACount and wifiAPCount

    /* Need to stringify device counts */
    snprintf(attribute_values[9], VAL_MAX_LEN, "%d", classicDeviceCount);
    snprintf(attribute_values[10], VAL_MAX_LEN, "%d", leDeviceCount);
    snprintf(attribute_values[11], VAL_MAX_LEN, "%d", wifiSTACount);
    snprintf(attribute_values[12], VAL_MAX_LEN, "%d", wifiAPCount);
}

void display_status_interactive(bool uuidDictionarySupported, bool btClassicSupported,
                                bool btBLESupported, bool wifiSupported) {
    const char *uuidDictionary = (uuidDictionarySupported) ? STRING_YES : STRING_NO;
    const char *btClassicSupport = (btClassicSupported) ? STRING_YES : STRING_NO;
    const char *btBLESupport = (btBLESupported) ? STRING_YES : STRING_NO;
    const char *wifiSupport = (wifiSupported) ? STRING_YES : STRING_NO;

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
    printf("WiFi Scanning: %28s", (!wifiSupported) ? STRING_NA : (scanStatus[SCAN_WIFI] == ACTION_ENABLE) ? STRING_ACTIVE : STRING_IDLE);
    print_status_row_end(4);
    print_empty_row(53);
    print_star(53, true);
    // Some extra stuff for development
    printf("sizeof(wendigo_bt_device): %d\tsizeof(wendigo_bt_svc): %d\n", sizeof(wendigo_bt_device), sizeof(wendigo_bt_svc));
    printf("sizeof(ScanType): %d\tsizeof(struct timeval): %d\n", sizeof(ScanType), sizeof(struct timeval));
    printf("sizeof(char*): %d\tsizeof(*all_gap_devices): %d\n", sizeof(char*), sizeof(all_gap_devices));

    free_status_details();
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

    repeat_bytes(0xEE, 4);
    repeat_bytes(0xBB, 4);

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

    repeat_bytes(0xAA, 4);
    repeat_bytes(0xFF, 4);

    free_status_details();
}
