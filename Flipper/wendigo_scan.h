#pragma once

#include "wendigo_app_i.h"
#include <sys/time.h>

#define BDA_LEN 6

void wendigo_set_scanning_active(WendigoApp *app, bool starting);
void wendigo_scan_handle_rx_data_cb(uint8_t* buf, size_t len, void* context);
void wendigo_free_uart_buffer();
void wendigo_esp_version(WendigoApp *app);

/* The following enum and structs MUST REMAIN IN SYNC with their counterparts in ESP32-Wendigo (bluetooth.h) */
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
    uint8_t bda[6];
    ScanType scanType;
    bool tagged;
    struct timeval lastSeen;
    wendigo_bt_svc bt_services;
} wendigo_bt_device;

/* Encapsulating struct so I have somewhere to store the class of device string */
typedef struct {
    wendigo_bt_device dev;
    char *cod_str;
} flipper_bt_device;
