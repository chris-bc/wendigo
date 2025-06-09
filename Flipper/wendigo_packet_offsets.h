/**
 * Structs have just blown my mind - based on its members wendigo_bt_device
 * (excluding wendigo_bt_svc) should be 45 bytes but it's 56. Wendigo_bt_svc
 * should be 10, is 16. I guess they're not just a chunk of memory containing
 * their members.
 * This means that a wendigo_bt_device can't be reliably sent and reconstructed
 * as a whole. So instead we'll pull the individual attributes out of the buffer
 * based on their offsets. This file defines their offsets into a packet, with
 * offset 0 representing the first byte of the packet preamble.
 * 
 * The offsets are only required for the Flipper Zero app
 */
#ifdef IS_FLIPPER_APP
 #define WENDIGO_OFFSET_BT_BDNAME_LEN           (8)
 #define WENDIGO_OFFSET_BT_EIR_LEN              (9)
 #define WENDIGO_OFFSET_BT_RSSI                 (10)
 #define WENDIGO_OFFSET_BT_COD                  (14)
 #define WENDIGO_OFFSET_BT_BDA                  (18)
 #define WENDIGO_OFFSET_BT_SCANTYPE             (24)
 #define WENDIGO_OFFSET_BT_TAGGED               (28)
 #define WENDIGO_OFFSET_BT_LASTSEEN             (29)
 #define WENDIGO_OFFSET_BT_NUM_SERVICES         (45)
 #define WENDIGO_OFFSET_BT_KNOWN_SERVICES_LEN   (46)
 #define WENDIGO_OFFSET_BT_COD_LEN              (47)
 #define WENDIGO_OFFSET_BT_BDNAME               (48)
 /* bdname is bdname_len bytes, followed by eir_len bytes of EIR and cod_len bytes of CoD */
 #endif

 typedef enum {
    SCAN_HCI = 0,
    SCAN_BLE,
    SCAN_WIFI,
    SCAN_INTERACTIVE,
    SCAN_TAG,
    SCAN_FOCUS,
    SCAN_COUNT
} ScanType;

typedef struct {
    uint16_t uuid16;
    char name[40];
} bt_uuid;

typedef struct {
    uint8_t num_services;
    void *service_uuids;        // Was esp_bt_uuid_t
    bt_uuid **known_services;
    uint8_t known_services_len;
} wendigo_bt_svc;

typedef struct {
    uint8_t bdname_len;
    uint8_t eir_len;
    int32_t rssi;
    uint32_t cod;
    uint8_t *eir;   // Consider inline - [ESP_BT_GAP_EIR_DATA_LEN]
    char *bdname;   // Consider inline - [ESP_BT_GAP_MAX_BDNAME_LEN + 1]
    uint8_t bda[MAC_BYTES];
    ScanType scanType;
    bool tagged;
    struct timeval lastSeen;
    wendigo_bt_svc bt_services;
} wendigo_bt_device;
