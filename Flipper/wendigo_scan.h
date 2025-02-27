#pragma once

#include "wendigo_app_i.h"

#define NUM_RADIOS (3)

bool wendigo_scan_start(WendigoApp* app);
bool wendigo_scan_wifi_start(WendigoApp* app);
bool wendigo_scan_bt_start(WendigoApp* app);
bool wendigo_scan_ble_start(WendigoApp* app);
bool wendigo_scan_wifi_stop(WendigoApp* app);
bool wendigo_scan_bt_stop(WendigoApp* app);
bool wendigo_scan_ble_stop(WendigoApp* app);
