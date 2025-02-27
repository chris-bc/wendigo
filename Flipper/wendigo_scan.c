#include "wendigo_scan.h"

bool wendigo_scan_start(WendigoApp* app) {
    // Get variables to store the active status of each radio (wifi, bt, ble)
    bool radios[NUM_RADIOS];
    for(int i = 0; i < NUM_RADIOS; ++i) {
        radios[i] = app->setup_selected_option_index[i] == RADIO_ON;
    }

    bool result = true;
    if(radios[SETUP_RADIO_WIFI_IDX]) {
        result = (result && wendigo_scan_wifi_start(app));
    }
    if(radios[SETUP_RADIO_BT_IDX]) {
        result = (result && wendigo_scan_bt_start(app));
    }
    if(radios[SETUP_RADIO_BLE_IDX]) {
        result = (result && wendigo_scan_ble_start(app));
    }

    return result;
}

bool wendigo_scan_stop(WendigoApp* app) {
    /* Get active status of each radio */
    bool radios[NUM_RADIOS];
    for(int i = 0; i < NUM_RADIOS; ++i) {
        radios[i] = app->setup_selected_option_index[i] == RADIO_ON;
    }

    bool result = true;
    if(radios[SETUP_RADIO_WIFI_IDX]) {
        result = (result && wendigo_scan_wifi_stop(app));
    }
    if(radios[SETUP_RADIO_BT_IDX]) {
        result = (result && wendigo_scan_bt_stop(app));
    }
    if(radios[SETUP_RADIO_BLE_IDX]) {
        result = (result && wendigo_scan_ble_stop(app));
    }

    return result;
}

bool wendigo_scan_wifi_start(WendigoApp* app) {
    UNUSED(app);
    return true;
}

bool wendigo_scan_bt_start(WendigoApp* app) {
    UNUSED(app);
    return true;
}

bool wendigo_scan_ble_start(WendigoApp* app) {
    UNUSED(app);
    return true;
}

bool wendigo_scan_wifi_stop(WendigoApp* app) {
    UNUSED(app);
    return true;
}

bool wendigo_app_bt_stop(WendigoApp* app) {
    UNUSED(app);
    return true;
}

bool wendigo_app_ble_stop(WendigoApp* app) {
    UNUSED(app);
    return true;
}
