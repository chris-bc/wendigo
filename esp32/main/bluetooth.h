#ifndef WENDIGO_BLUETOOTH_H
#define WENDIGO_BLUETOOTH_H

#include "common.h"

typedef struct {
    //esp_bt_uuid_t *uuid;
    uint16_t uuid16;
    char name[40];
} bt_uuid;

#endif