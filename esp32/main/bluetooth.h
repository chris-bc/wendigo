#ifndef WENDIGO_BLUETOOTH_H
#define WENDIGO_BLUETOOTH_H

#include "common.h"

#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_gap_ble_api.h>
#include <esp_gatt_defs.h>
#include <esp_gattc_api.h>
#include <esp_gap_bt_api.h>
#include <esp_gatt_common_api.h>

#define COD_MAX_LEN 59
#define SHORT_COD_MAX_LEN 11

static const char *BT_TAG = "HCI@Wendigo";
static const char *BLE_TAG = "BLE@Wendigo";

typedef struct {
    //esp_bt_uuid_t *uuid;
    uint16_t uuid16;
    char name[40];
} bt_uuid;

typedef struct {
    uint8_t num_services;
    esp_bt_uuid_t *service_uuids;
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
    esp_bd_addr_t bda;
    ScanType scanType;
    wendigo_bt_svc bt_services;
} wendigo_bt_device;

char *bda2str(esp_bd_addr_t bda, char *str, size_t size);

esp_err_t wendigo_bt_initialise();
esp_err_t wendigo_bt_enable();
esp_err_t wendigo_bt_disable();
esp_err_t wendigo_ble_enable();
esp_err_t wendigo_ble_disable();
esp_err_t display_gap_device(wendigo_bt_device *dev);

#endif