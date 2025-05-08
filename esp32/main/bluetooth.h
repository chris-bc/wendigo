#ifndef WENDIGO_BLUETOOTH_H
#define WENDIGO_BLUETOOTH_H

#include "common.h"

#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_gap_ble_api.h>
#include <esp_gatt_defs.h>
#include <esp_gattc_api.h>
#include <esp_gap_bt_api.h>

typedef struct {
    //esp_bt_uuid_t *uuid;
    uint16_t uuid16;
    char name[40];
} bt_uuid;

char *bda2str(esp_bd_addr_t bda, char *str, size_t size);

esp_err_t wendigo_bt_initialise();
esp_err_t wendigo_bt_enable();
esp_err_t wendigo_bt_disable();
esp_err_t wendigo_ble_enable();
esp_err_t wendigo_ble_disable();

#endif