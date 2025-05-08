#ifndef WENDIGO_BLUETOOTH_H
#define WENDIGO_BLUETOOTH_H

#include "common.h"
// TODO: Decide whether #includes like the following should live here or in the .c file (so libraries are not available to other modules)
#include <esp_bt.h>
#include <esp_bt_main.h>

typedef struct {
    //esp_bt_uuid_t *uuid;
    uint16_t uuid16;
    char name[40];
} bt_uuid;

esp_err_t wendigo_bt_initialise();
esp_err_t wendigo_bt_enable();
esp_err_t wendigo_bt_disable();
esp_err_t wendigo_ble_enable();
esp_err_t wendigo_ble_disable();

#endif