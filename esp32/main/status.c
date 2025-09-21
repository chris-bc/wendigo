#include "status.h"
#include "common.h"
#include "portmacro.h"

#define NAME_MAX_LEN   (uint8_t)35
#define VAL_MAX_LEN    (uint8_t)20
#define ATTR_COUNT_MAX (uint8_t)14 // TODO: Is there a reason this isn't part of the enum?

char *attribute_names[] = {"Version:", "Chris Bennetts-Cash",
    STRING_BT_UUID_DICTIONARY, STRING_BT_CLASSIC_SUPPORTED, STRING_BLE_SUPPORTED,
    STRING_WIFI_24_SUPPORTED, STRING_WIFI_5_SUPPORTED, STRING_BT_CLASSIC_SCANNING,
    STRING_BLE_SCANNING, STRING_WIFI_SCANNING, STRING_BT_CLASSIC_COUNT,
    STRING_BLE_COUNT, STRING_WIFI_STA_COUNT, STRING_WIFI_AP_COUNT};
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
    ATTR_WIFI_24_SUPPORT,
    ATTR_WIFI_5_SUPPORT,
    ATTR_BT_CLASSIC_SCANNING,
    ATTR_BT_BLE_SCANNING,
    ATTR_WIFI_SCANNING,
    ATTR_BT_CLASSIC_COUNT,
    ATTR_BT_BLE_COUNT,
    ATTR_WIFI_STA_COUNT,
    ATTR_WIFI_AP_COUNT,
};

/** Prepares data for display by the status command.
 * The function populates attribute_values[].
 * featureMask is a combination of SupportedHardwareMask
 * values combined with logical OR.
 */
void initialise_status_details(uint8_t featureMask) {
    const char github[] = "github.com/chris-bc";

    /* strncpy doesn't always add the terminating null byte, so start with a sequence of null bytes */
    for (uint8_t i = 0; i < ATTR_COUNT_MAX; ++i) {
        explicit_bzero(attribute_values[i], VAL_MAX_LEN);
    }

    strncpy(attribute_values[ATTR_VERSION], WENDIGO_VERSION, strlen(WENDIGO_VERSION) + 1);
    strncpy(attribute_values[ATTR_GITHUB], github, strlen(github));
    strncpy(attribute_values[ATTR_UUID_DICTIONARY],
        (featureMask & HW_BT_UUID_DICTIONARY) == HW_BT_UUID_DICTIONARY ?
        STRING_YES : STRING_NO, VAL_MAX_LEN);
    strncpy(attribute_values[ATTR_BT_CLASSIC_SUPPORT],
        (featureMask & HW_BT_CLASSIC_SUPPORTED) == HW_BT_CLASSIC_SUPPORTED ?
        STRING_YES : STRING_NO, VAL_MAX_LEN);
    strncpy(attribute_values[ATTR_BT_BLE_SUPPORT],
        (featureMask & HW_BLE_SUPPORTED) == HW_BLE_SUPPORTED ? STRING_YES :
        STRING_NO, VAL_MAX_LEN);
    strncpy(attribute_values[ATTR_WIFI_24_SUPPORT],
        (featureMask & HW_WIFI_24_SUPPORTED) == HW_WIFI_24_SUPPORTED ?
        STRING_YES : STRING_NO, VAL_MAX_LEN);
    strncpy(attribute_values[ATTR_WIFI_5_SUPPORT],
        (featureMask &  HW_WIFI_5_SUPPORTED) == HW_WIFI_5_SUPPORTED ?
        STRING_YES : STRING_NO, VAL_MAX_LEN);
    strncpy(attribute_values[ATTR_BT_CLASSIC_SCANNING],
        (scanStatus[SCAN_HCI] == ACTION_ENABLE) ? STRING_ACTIVE : STRING_IDLE,
        VAL_MAX_LEN);
    strncpy(attribute_values[ATTR_BT_BLE_SCANNING],
        (scanStatus[SCAN_BLE] == ACTION_ENABLE) ? STRING_ACTIVE : STRING_IDLE,
        VAL_MAX_LEN);
    strncpy(attribute_values[ATTR_WIFI_SCANNING],
        (scanStatus[SCAN_WIFI_AP] == ACTION_ENABLE ||
            scanStatus[SCAN_WIFI_STA] == ACTION_ENABLE) ?
        STRING_ACTIVE : STRING_IDLE, VAL_MAX_LEN);
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

void display_status_interactive() {
    /* Some defines for the layout */
    #define ROW_LEN         (53)
    #define MARGIN_LEN      (4)
    #define TEXT_LEN        (ROW_LEN - (2 * (MARGIN_LEN + 1)))

    uint8_t supported = wendigo_supported_features();
    bool uuidDictionarySupported = ((supported & HW_BT_UUID_DICTIONARY) == HW_BT_UUID_DICTIONARY);
    bool btClassicSupported = ((supported & HW_BT_CLASSIC_SUPPORTED) == HW_BT_CLASSIC_SUPPORTED);
    bool btBLESupported = ((supported & HW_BLE_SUPPORTED) == HW_BLE_SUPPORTED);
    bool wifi24Supported = ((supported & HW_WIFI_24_SUPPORTED) == HW_WIFI_24_SUPPORTED);
    bool wifi5Supported = ((supported & HW_WIFI_5_SUPPORTED) == HW_WIFI_5_SUPPORTED);
    const char *uuidDictionary = (uuidDictionarySupported) ? STRING_YES : STRING_NO;
    const char *btClassicSupport = (btClassicSupported) ? STRING_YES : STRING_NO;
    const char *btBLESupport = (btBLESupported) ? STRING_YES : STRING_NO;
    const char *wifi24Support = (wifi24Supported) ? STRING_YES : STRING_NO;
    const char *wifi5Support = (wifi5Supported) ? STRING_YES : STRING_NO;
    char strDeviceCount[5]; /* Temp storage for device counts as strings */
    /* While we're not using attribute_names[] and attribute_values[], this
       function also generates device counts for different device types. */
    initialise_status_details(uuidDictionarySupported, btClassicSupported, btBLESupported, wifiSupported);

    print_star(ROW_LEN, true);
    print_empty_row(ROW_LEN;
    // TODO: Remove magic numbers from these
    print_row_start(14);
    printf("Wendigo version %7s", WENDIGO_VERSION);
    print_row_end(14);
    print_empty_row(ROW_LEN);
    print_row_start(5);
    printf("Chris Bennetts-Cash   github.com/chris-bc");
    print_row_end(5);
    print_empty_row(ROW_LEN);
    print_empty_row(ROW_LEN);
    print_row_start(MARGIN_LEN);
    /* Keep track of the number of spaces needed to justify content */
    uint8_t spaceCount = TEXT_LEN - strlen(STRING_BT_UUID_DICTIONARY) - strlen(uuidDictionary);
    printf("%s", STRING_BT_UUID_DICTIONARY);
    print_space(spaceCount, false);
    printf("%s", uuidDictionary);
    print_row_end(MARGIN_LEN);
    print_row_start(MARGIN_LEN);
    spaceCount = TEXT_LEN - strlen(STRING_BT_CLASSIC_SUPPORTED) - strlen(btClassicSupport);
    printf("%s", STRING_BT_CLASSIC_SUPPORTED);
    print_space(spaceCount, false);
    printf("%s", btClassicSupport);
    print_row_end(MARGIN_LEN);
    print_row_start(MARGIN_LEN);
    spaceCount = TEXT_LEN - strlen(STRING_BLE_SUPPORTED) - strlen(btBLESupport);
    printf("%s", STRING_BLE_SUPPORTED);
    print_space(spaceCount, false);
    printf("%s", btBLESupport);
    print_row_end(MARGIN_LEN);
    print_row_start(MARGIN_LEN);
    spaceCount = TEXT_LEN - strlen(STRING_WIFI_24_SUPPORTED) - strlen(wifi24Support);
    printf("%s", STRING_WIFI_24_SUPPORTED);
    print_space(spaceCount, false);
    printf("%s", wifi24Support);
    print_row_end(MARGIN_LEN);
    print_row_start(MARGIN_LEN);
    spaceCount = TEXT_LEN - strlen(STRING_WIFI_5_SUPPORTED) - strlen(wifi5Support);
    printf("%s", STRING_WIFI_5_SUPPORTED);
    print_space(spaceCount, false);
    printf("%s", wifi5Support);
    print_row_end(MARGIN_LEN);
    // TODO: Update these
    print_row_start(MARGIN_LEN);
    printf("Bluetooth Classic Scanning: %15s", (!btClassicSupported) ? STRING_NA : (scanStatus[SCAN_HCI] == ACTION_ENABLE) ? STRING_ACTIVE : STRING_IDLE);
    print_row_end(MARGIN_LEN);
    print_row_start(MARGIN_LEN);
    printf("Bluetooth Low Energy Scanning: %12s", (!btBLESupported) ? STRING_NA : (scanStatus[SCAN_BLE] == ACTION_ENABLE) ? STRING_ACTIVE : STRING_IDLE);
    print_row_end(MARGIN_LEN);
    print_row_start(MARGIN_LEN);
    printf("WiFi Scanning: %28s", (!wifiSupported) ? STRING_NA : (scanStatus[SCAN_WIFI_AP] == ACTION_ENABLE || scanStatus[SCAN_WIFI_STA] == ACTION_ENABLE) ? STRING_ACTIVE : STRING_IDLE);
    print_row_end(MARGIN_LEN);
    print_row_start(MARGIN_LEN);
    explicit_bzero(strDeviceCount, sizeof(strDeviceCount));
    snprintf(strDeviceCount, sizeof(strDeviceCount), "%d", classicDeviceCount);
    spaceCount = TEXT_LEN - strlen(STRING_BT_CLASSIC_COUNT) - strlen(strDeviceCount);
    printf("%s", STRING_BT_CLASSIC_COUNT);
    print_space(spaceCount, false);
    printf("%s", strDeviceCount);
    print_row_end(MARGIN_LEN);
    print_row_start(MARGIN_LEN);
    explicit_bzero(strDeviceCount, sizeof(strDeviceCount));
    snprintf(strDeviceCount, sizeof(strDeviceCount), "%d", leDeviceCount);
    spaceCount = TEXT_LEN - strlen(STRING_BLE_COUNT) - strlen(strDeviceCount);
    printf("%s", STRING_BLE_COUNT);
    print_space(spaceCount, false);
    printf("%s", strDeviceCount);
    print_row_end(MARGIN_LEN);
    print_row_start(MARGIN_LEN);
    explicit_bzero(strDeviceCount, sizeof(strDeviceCount));
    snprintf(strDeviceCount, sizeof(strDeviceCount), "%d", wifiAPCount);
    spaceCount = TEXT_LEN - strlen(STRING_WIFI_AP_COUNT) - strlen(strDeviceCount);
    printf("%s", STRING_WIFI_AP_COUNT);
    print_space(spaceCount, false);
    printf("%s", strDeviceCount);
    print_row_end(MARGIN_LEN);
    print_row_start(MARGIN_LEN);
    explicit_bzero(strDeviceCount, sizeof(strDeviceCount));
    snprintf(strDeviceCount, sizeof(strDeviceCount), "%d", wifiSTACount);
    spaceCount = TEXT_LEN - strlen(STRING_WIFI_STA_COUNT) - strlen(strDeviceCount);
    printf("%s", STRING_WIFI_STA_COUNT);
    print_space(spaceCount, false);
    printf("%s", strDeviceCount);
    print_row_end(MARGIN_LEN);
    print_empty_row(ROW_LEN);
    print_star(ROW_LEN, true);
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
void display_status_uart() {
    /* Get features supported by the ESP32 chip */
    uint8_t supported = wendigo_supported_features();
    bool uuidDictionarySupported = ((supported & HW_BT_UUID_DICTIONARY) == HW_BT_UUID_DICTIONARY);
    bool btClassicSupported = ((supported & HW_BT_CLASSIC_SUPPORTED) == HW_BT_CLASSIC_SUPPORTED);
    bool btBLESupported = ((supported & HW_BLE_SUPPORTED) == HW_BLE_SUPPORTED);
    bool wifiSupported = ((supported & HW_WIFI_SUPPORTED) != 0);
    initialise_status_details(uuidDictionarySupported, btClassicSupported, btBLESupported, wifiSupported);

    if (xSemaphoreTake(uartMutex, portMAX_DELAY)) { /* Wait for the talking stick */
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
        xSemaphoreGive(uartMutex);
    }
}
