#ifndef WENDIGO_BLUETOOTH_H
#define WENDIGO_BLUETOOTH_H

#include "common.h"

#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_gattc_api.h>
#include <esp_gap_bt_api.h>
#include <esp_gatt_common_api.h>

#define COD_MAX_LEN 59
#define SHORT_COD_MAX_LEN 11

static const char *BT_TAG = "HCI@Wendigo";
static const char *BLE_TAG = "BLE@Wendigo";

char *bda2str(esp_bd_addr_t bda, char *str, size_t size);

esp_err_t wendigo_bt_initialise();
esp_err_t wendigo_bt_enable();
esp_err_t wendigo_bt_disable();
esp_err_t wendigo_ble_enable();
esp_err_t wendigo_ble_disable();
esp_err_t display_gap_device(wendigo_device *dev);
wendigo_device *retrieve_device(wendigo_device *dev);
wendigo_device *retrieve_by_mac(esp_bd_addr_t bda);

#endif