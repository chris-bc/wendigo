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

 typedef enum {
    WIFI_AUTH_OPEN = 0,         /**< Authenticate mode : open */
    WIFI_AUTH_WEP,              /**< Authenticate mode : WEP */
    WIFI_AUTH_WPA_PSK,          /**< Authenticate mode : WPA_PSK */
    WIFI_AUTH_WPA2_PSK,         /**< Authenticate mode : WPA2_PSK */
    WIFI_AUTH_WPA_WPA2_PSK,     /**< Authenticate mode : WPA_WPA2_PSK */
    WIFI_AUTH_ENTERPRISE,       /**< Authenticate mode : Wi-Fi EAP security */
    WIFI_AUTH_WPA2_ENTERPRISE = WIFI_AUTH_ENTERPRISE,  /**< Authenticate mode : Wi-Fi EAP security */
    WIFI_AUTH_WPA3_PSK,         /**< Authenticate mode : WPA3_PSK */
    WIFI_AUTH_WPA2_WPA3_PSK,    /**< Authenticate mode : WPA2_WPA3_PSK */
    WIFI_AUTH_WAPI_PSK,         /**< Authenticate mode : WAPI_PSK */
    WIFI_AUTH_OWE,              /**< Authenticate mode : OWE */
    WIFI_AUTH_WPA3_ENT_192,     /**< Authenticate mode : WPA3_ENT_SUITE_B_192_BIT */
    WIFI_AUTH_WPA3_EXT_PSK,     /**< This authentication mode will yield same result as WIFI_AUTH_WPA3_PSK and
 not recommended to be used. It will be deprecated in future, please use WIFI_AUTH_WPA3_PSK instead. */
    WIFI_AUTH_WPA3_EXT_PSK_MIXED_MODE, /**< This authentication mode will yield same result as WIFI_AUTH_WPA3_
PSK and not recommended to be used. It will be deprecated in future, please use WIFI_AUTH_WPA3_PSK instead.*/
    WIFI_AUTH_DPP,              /**< Authenticate mode : DPP */
    WIFI_AUTH_WPA3_ENTERPRISE,  /**< Authenticate mode : WPA3-Enterprise Only Mode */
    WIFI_AUTH_WPA2_WPA3_ENTERPRISE, /**< Authenticate mode : WPA3-Enterprise Transition Mode */
    WIFI_AUTH_MAX
} wifi_auth_mode_t;
 #endif

#define MAX_SSID_LEN    32
#define MAC_STRLEN      17
#define MAC_BYTES       6

 typedef enum {
    SCAN_HCI = 0,
    SCAN_BLE,
    SCAN_WIFI_AP,
    SCAN_WIFI_STA,
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
    #ifdef IS_FLIPPER_APP
        void *service_uuids;        // Was esp_bt_uuid_t
    #else
        esp_bt_uuid_t *service_uuids;
    #endif
    bt_uuid **known_services;
    uint8_t known_services_len;
} wendigo_bt_svc;

typedef struct {
    uint8_t bdname_len;
    uint8_t eir_len;
    uint32_t cod;
    char *cod_str;
    uint8_t *eir;   // Consider inline - [ESP_BT_GAP_EIR_DATA_LEN]
    char *bdname;   // Consider inline - [ESP_BT_GAP_MAX_BDNAME_LEN + 1]
    wendigo_bt_svc bt_services;
} wendigo_bt_device;

typedef struct wendigo_wifi_ap {
    void **stations;
    uint8_t stations_count;
    uint8_t ssid[MAX_SSID_LEN + 1];                     /** SSID of AP */
    uint8_t channel;
    wifi_auth_mode_t authmode;
    uint32_t phy_11b: 1;                  /**< Bit: 0 flag to identify if 11b mode is enabled or not */
    uint32_t phy_11g: 1;                  /**< Bit: 1 flag to identify if 11g mode is enabled or not */
    uint32_t phy_11n: 1;                  /**< Bit: 2 flag to identify if 11n mode is enabled or not */
    uint32_t phy_lr: 1;                   /**< Bit: 3 flag to identify if low rate is enabled or not */
    uint32_t phy_11a: 1;                  /**< Bit: 4 flag to identify if 11ax mode is enabled or not */
    uint32_t phy_11ac: 1;                 /**< Bit: 5 flag to identify if 11ax mode is enabled or not */
    uint32_t phy_11ax: 1;                 /**< Bit: 6 flag to identify if 11ax mode is enabled or not */
    uint32_t wps: 1;                      /**< Bit: 7 flag to identify if WPS is supported or not */
    uint32_t ftm_responder: 1;            /**< Bit: 8 flag to identify if FTM is supported in responder mode */
    uint32_t ftm_initiator: 1;            /**< Bit: 9 flag to identify if FTM is supported in initiator mode */
    uint32_t reserved: 22;                /**< Bit: 10..31 reserved */
} wendigo_wifi_ap;

typedef struct wendigo_wifi_sta {
    wendigo_wifi_ap *ap;
    uint8_t apMac[MAC_BYTES];
    uint8_t channel;
} wendigo_wifi_sta;

typedef struct wendigo_device {
    uint8_t mac[MAC_BYTES];
    int8_t rssi;
    ScanType scanType;
    bool tagged;
    struct timeval lastSeen;
    #ifdef IS_FLIPPER_APP
        VariableItem *view;
    #endif
    union {
        wendigo_bt_device bluetooth;
        wendigo_wifi_ap ap;
        wendigo_wifi_sta sta;
    } radio;
} wendigo_device;
