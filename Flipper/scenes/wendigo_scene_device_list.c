#include "../wendigo_app_i.h"
#include "../wendigo_scan.h"

/* Public method from wendigo_scene_device_detail.c */
extern void wendigo_scene_device_detail_set_device(wendigo_device *d);
/* Internal method - I don't wan't to move all calling functions below it */
static void wendigo_scene_device_list_var_list_change_callback(VariableItem* item);

/* This scene is used to both display all devices and display selected devices */
bool display_selected_only = false;

/* Devices currently being displayed */
wendigo_device **current_devices = NULL;
uint16_t current_devices_count = 0;

/* Enum to index the options menu for devices */
enum wendigo_device_list_bt_options {
    WendigoOptionBTRSSI = 0,
    WendigoOptionBTTagUntag,
    WendigoOptionBTScanType,
    WendigoOptionBTCod,
    WendigoOptionBTLastSeen,
    WendigoOptionsBTCount
};

enum wendigo_device_list_ap_options {
    WendigoOptionAPRSSI = 0,
    WendigoOptionAPTagUntag,
    WendigoOptionAPScanType,
    WendigoOptionAPAuthMode,
    WendigoOptionAPChannel,
    WendigoOptionAPLastSeen,
    WendigoOptionsAPCount
};

enum wendigo_device_list_sta_options {
    WendigoOptionSTARSSI = 0,
    WendigoOptionSTATagUntag,
    WendigoOptionSTAScanType,
    WendigoOptionSTAAP,
    WendigoOptionSTAChannel,
    WendigoOptionSTALastSeen,
    WendigoOptionsSTACount
};

/** Restrict displayed devices based on the specified filters
 * deviceMask: A bitmask of DeviceMask values. e.g. DEVICE_WIFI_AP | DEVICE_WIFI_STA to display all WiFi devices.
 *             0 to display all device types.
 * tagged: true if only tagged devices are to be displayed
 */
uint16_t wendigo_set_devices(uint8_t deviceMask, bool tagged) {
    if (deviceMask == 0) {
        deviceMask = DEVICE_ALL;
    }
    /* To ensure we only malloc the required memory, run an initial pass to count the number of devices */
    uint16_t deviceCount = 0;
    for (uint16_t idx = 0; idx < devices_count; ++idx) {
        if ((devices[idx]->scanType == SCAN_HCI && ((deviceMask & DEVICE_BT_CLASSIC) == DEVICE_BT_CLASSIC) && (!tagged || (tagged && devices[idx]->tagged))) ||
            (devices[idx]->scanType == SCAN_BLE && ((deviceMask & DEVICE_BT_LE) == DEVICE_BT_LE) && (!tagged || (tagged && devices[idx]->tagged))) ||
            (devices[idx]->scanType == SCAN_WIFI_AP && ((deviceMask & DEVICE_WIFI_AP) == DEVICE_WIFI_AP) && (!tagged || (tagged && devices[idx]->tagged))) ||
            (devices[idx]->scanType == SCAN_WIFI_STA && ((deviceMask & DEVICE_WIFI_STA) == DEVICE_WIFI_STA) && (!tagged || (tagged && devices[idx]->tagged)))) {
                ++deviceCount;
            }
    }
    wendigo_device **new_devices = realloc(current_devices, sizeof(wendigo_device *) * deviceCount);
    if (new_devices == NULL) {
        return false;
    }
    current_devices = new_devices;
    current_devices_count = deviceCount;
    /* Populate current_devices[] */
    uint16_t current_index = 0;
    for (uint16_t i = 0; i < devices_count; ++i) {
        if ((devices[i]->scanType == SCAN_HCI && ((deviceMask & DEVICE_BT_CLASSIC) == DEVICE_BT_CLASSIC) && (!tagged || (tagged && devices[i]->tagged))) ||
            (devices[i]->scanType == SCAN_BLE && ((deviceMask & DEVICE_BT_LE) == DEVICE_BT_LE) && (!tagged || (tagged && devices[i]->tagged))) ||
            (devices[i]->scanType == SCAN_WIFI_AP && ((deviceMask & DEVICE_WIFI_AP) == DEVICE_WIFI_AP) && (!tagged || (tagged && devices[i]->tagged))) ||
            (devices[i]->scanType == SCAN_WIFI_STA && ((deviceMask & DEVICE_WIFI_STA) == DEVICE_WIFI_STA) && (!tagged || (tagged && devices[i]->tagged)))) {
                current_devices[current_index++] = devices[i];
        }
    }
    furi_assert(current_index == deviceCount);
    return deviceCount;
}

/** A more flexible version of elapsedTime() that lets us avoid running furi_hal_rtc_get_timestamp().
 * This version is suitable for running at high frequency - In tick events etc.
 * `from` and `to` are seconds since the Unix Epoch. Returns elapsed seconds and, if elapsedStr is an
 * initialised char[] and strlen > 0, places a text representation of the elapsed time in elapsedStr.
 * Returns zero and the empty string on failure.
 */
double _elapsedTime(uint32_t *from, uint32_t *to, char *elapsedStr, uint8_t strlen) {
    /* Validate everything before we touch it */
    if (from == NULL || to == NULL) {
        if (elapsedStr != NULL && strlen > 0) {
            elapsedStr[0] = '\0';
        }
        return 0;
    }
    uint16_t elapsed = (uint16_t)(*to - *from);
    if (elapsedStr != NULL && strlen > 0) {
        if (elapsed < 60) {
            snprintf(elapsedStr, strlen, "%ds", elapsed);
        } else {
            uint8_t minutes = elapsed / 60;
            uint8_t seconds = elapsed - (minutes * 60);
            snprintf(elapsedStr, strlen, "%2d:%02d", minutes, seconds);
        }
    }
    return elapsed;
}

/** Calculate the elapsed time since the specified device was last seen.
 * Returns the elapsed time as a uint32 and, if elapsedStr is not NULL,
 * in a string representation there. elapsedStr must be an initialised
 * char[] of at least 7 bytes.
 * If a string representation is not needed provide NULL for elapsedStr.
 * Returns zero and an empty string on failure.
 */
double elapsedTime(wendigo_device *dev, char *elapsedStr, uint8_t strlen) {
    if (dev == NULL) {
        /* Return sensible values on failure */
        if (strlen > 0 && elapsedStr != NULL) {
            elapsedStr[0] = '\0';
        }
        return 0;
    }
    uint32_t nowTime = furi_hal_rtc_get_timestamp();
    return _elapsedTime((uint32_t *)&(dev->lastSeen.tv_sec), &nowTime, elapsedStr, strlen);
}

/** Identify the device represented by the currently-selected menu item. NULL if it cannot be identified. */
wendigo_device *wendigo_scene_device_list_selected_device(VariableItem *item) {
    WendigoApp* app = variable_item_get_context(item);

    wendigo_device **cache = (display_selected_only) ? selected_devices : devices;
    uint16_t cache_count = (display_selected_only) ? selected_devices_count : devices_count;
    if (current_devices_count == 0 || current_devices == NULL) {
        wendigo_set_devices(0, false);
    }
    if (app->device_list_selected_menu_index < cache_count) {
        return cache[app->device_list_selected_menu_index];
    }
// TODO: Compare these methods - I'm tempted to remove the first approach (which is why it now gets its own function)
    /* Instead of indexing into the array, see if we can find the device with a reverse lookup from `item` */
    uint16_t idx = 0;
    for (; idx < cache_count && cache[idx]->view != item; ++idx) { }
    if (idx < cache_count) {
        return cache[idx];
    }
    /* Device not found */
    return NULL;
}

/** Update the current display to reflect a new Bluetooth discovery result for `dev`.
 * This function is called by the functions wendigo_add_bt_device() and wendigo_update_bt_device()
 * in wendigo_scan.c. When this scene and scanning are active at the same time all devices
 * identified, whether newly-identified devices or subsequent packets describing a known device,
 * are passed as an argument to this function to allow the UI to be dynamically updated and to display
 * either information about a new device or updated information about an existing device.
 */
void wendigo_scene_device_list_update(WendigoApp *app, wendigo_device *dev) {
    char *name;
    /* Use bdname as name if it's a bluetooth device and we have a name */
    if ((dev->scanType == SCAN_HCI || dev->scanType == SCAN_BLE) &&
            dev->radio.bluetooth.bdname_len > 0 && dev->radio.bluetooth.bdname != NULL) {
        name = dev->radio.bluetooth.bdname;
    } else if (dev->scanType == SCAN_WIFI_AP && dev->radio.ap.ssid[0] != '\0') {
        /* Use access point SSID if it's an AP and we have an SSID */
        name = (char *)dev->radio.ap.ssid;
    } else {
        /* Use MAC to label the device */
        name = malloc(sizeof(char) * (MAC_STRLEN + 1));
        if (name == NULL) {
            // TODO: Panic
            return;
        }
        bytes_to_string(dev->mac, MAC_BYTES, name);
    }
    char tempStr[10];
    if (dev->view == NULL && !display_selected_only) {
        /* Add a new item if we're not displaying tagged items only */
        dev->view = variable_item_list_add(
            app->devices_var_item_list,
            name,
            WendigoOptionsBTCount,
            wendigo_scene_device_list_var_list_change_callback,
            app);
        /* Display RSSI in options menu */
        variable_item_set_current_value_index(dev->view, WendigoOptionBTRSSI);
        snprintf(tempStr, sizeof(tempStr), "%d dB", dev->rssi);
        variable_item_set_current_value_text(dev->view, tempStr);
    } else if (dev->view != NULL) {
        /* Update dev->view */
        variable_item_set_item_label(dev->view, name);
        /* If RSSI or lastSeen is displayed, update their values */
        if (variable_item_get_current_value_index(dev->view) == WendigoOptionBTRSSI) {
            snprintf(tempStr, sizeof(tempStr), "%d dB", dev->rssi);
            variable_item_set_current_value_text(dev->view, tempStr);
        } else if (variable_item_get_current_value_index(dev->view) == WendigoOptionBTLastSeen) {
            elapsedTime(dev, tempStr, sizeof(tempStr));
            variable_item_set_current_value_text(dev->view, tempStr);
        }// TODO: Support multiple device types, update AP/channel for STA if changed, update SSID/channel for AP if changed
    } /* Ignore new devices if display_selected_only is true */
    /* Free `name` if we malloc'd it */
    if (((dev->scanType == SCAN_HCI || dev->scanType == SCAN_BLE) &&
                (dev->radio.bluetooth.bdname_len == 0 || dev->radio.bluetooth.bdname == NULL)) ||
            (dev->scanType == SCAN_WIFI_AP && dev->radio.ap.ssid[0] == '\0') ||
            (dev->scanType != SCAN_WIFI_AP && dev->scanType != SCAN_HCI && dev->scanType != SCAN_BLE)) {
        free(name);    
    }
}

/** Re-render the variable item list. This function exists because there is no method to remove
 * items from a variable_item_list, but that is sometimes necessary (e.g. when viewing selected
 * devices and de-selecting a device).
 */
void wendigo_scene_device_list_redraw(WendigoApp *app) {
    variable_item_list_reset(app->devices_var_item_list);
    char *item_str;
    uint16_t my_devices_count;
    wendigo_device **my_devices;
    if (display_selected_only) {
        my_devices_count = selected_devices_count;
        my_devices = selected_devices;
    } else {
        my_devices_count = devices_count;
        my_devices = devices;
    }
    variable_item_list_set_selected_item(app->devices_var_item_list, 0);
    for(uint16_t i = 0; i < my_devices_count; ++i) {
        /* Label with bdname if it's bluetooth & we have a name */
        if ((my_devices[i]->scanType == SCAN_HCI || my_devices[i]->scanType == SCAN_BLE) &&
                my_devices[i]->radio.bluetooth.bdname_len > 0 &&
                my_devices[i]->radio.bluetooth.bdname != NULL) {
            item_str = my_devices[i]->radio.bluetooth.bdname;
        /* Label with SSID if it's an AP and we have an SSID */
        } else if (my_devices[i]->scanType == SCAN_WIFI_AP && my_devices[i]->radio.ap.ssid[0] != '\0') {
            item_str = (char *)my_devices[i]->radio.ap.ssid;
        /* Otherwise use the MAC/BDA */
        } else {
            item_str = malloc(sizeof(char) * (MAC_STRLEN + 1));
            if (item_str == NULL) {
                // TODO: Panic
                continue; // Maybe the next device will have a name so we won't need to malloc
            }
            bytes_to_string(my_devices[i]->mac, MAC_BYTES, item_str);
        }
        my_devices[i]->view = variable_item_list_add(
            app->devices_var_item_list,
            item_str,
            WendigoOptionsBTCount,
            wendigo_scene_device_list_var_list_change_callback,
            app);
        /* free item_str if we malloc'd it */
        if (((my_devices[i]->scanType == SCAN_HCI || my_devices[i]->scanType == SCAN_BLE) &&
                    (my_devices[i]->radio.bluetooth.bdname_len == 0 ||
                     my_devices[i]->radio.bluetooth.bdname == NULL)) ||
                (my_devices[i]->scanType == SCAN_WIFI_AP && my_devices[i]->radio.ap.ssid[0] == '\0') ||
                (my_devices[i]->scanType != SCAN_HCI && my_devices[i]->scanType != SCAN_BLE &&
                 my_devices[i]->scanType != SCAN_WIFI_AP)) {
            free(item_str);
        }
        /* Default to displaying RSSI in options menu */
        char tempStr[10];
        snprintf(tempStr, sizeof(tempStr), "%d dB", my_devices[i]->rssi);
        variable_item_set_current_value_index(my_devices[i]->view, WendigoOptionBTRSSI);
        variable_item_set_current_value_text(my_devices[i]->view, tempStr);
    }
    // TODO: Display WiFi devices
}

static void wendigo_scene_device_list_var_list_enter_callback(void* context, uint32_t index) {
    furi_assert(context);
    WendigoApp* app = context;

    furi_assert(index < ((display_selected_only) ? selected_devices_count : devices_count));

    wendigo_device *item = (display_selected_only) ? selected_devices[index] : devices[index];
    if (item == NULL) {
        return;
    }

    app->device_list_selected_menu_index = index;

    /* If the tag/untag menu item is selected perform that action, otherwise display details for `item` */
    if (item->view != NULL && variable_item_get_current_value_index(item->view) == WendigoOptionBTTagUntag) {
        wendigo_set_device_selected(item, !item->tagged);
        variable_item_set_current_value_text(item->view, (item->tagged) ? "Untag" : "Tag");
        /* If the device is now untagged and we're viewing tagged devices only, remove the device from view */
        if (display_selected_only && !item->tagged) {
            /* Bugger - There's no method to remove an item from a variable_item_list */
            item->view = NULL;
            wendigo_scene_device_list_redraw(app);
        }
    } else {
        /* Display details for `item` */
        wendigo_scene_device_detail_set_device(item);
        // TODO Fix detail scene, then view_dispatcher_send_custom_event(app->view_dispatcher, Wendigo_EventListDevices);
    }
}

static void wendigo_scene_device_list_var_list_change_callback(VariableItem* item) {
    furi_assert(item);

    /* app->device_list_selected_menu_index should reliably point to the selected menu item.
       Use that to obtain the flipper_bt_device* */
    wendigo_device *menu_item = wendigo_scene_device_list_selected_device(item);

    if (menu_item != NULL) {
        uint8_t option_index = variable_item_get_current_value_index(item);
        furi_assert(option_index < WendigoOptionsBTCount);
        /* Update the current option label */
        char tempStr[16];
        switch (option_index) {
            case WendigoOptionBTRSSI:
                snprintf(tempStr, sizeof(tempStr), "%d dB", menu_item->rssi);
                variable_item_set_current_value_text(item, tempStr);
                break;
            case WendigoOptionBTTagUntag:
                variable_item_set_current_value_text(item, (menu_item->tagged) ? "Untag" : "Tag");
                break;
            case WendigoOptionBTScanType:
                variable_item_set_current_value_text(item, (menu_item->scanType == SCAN_HCI) ? "BT Classic" :
                    (menu_item->scanType == SCAN_BLE) ? "BLE" : (menu_item->scanType == SCAN_WIFI_AP) ?
                    "WiFi AP" : (menu_item->scanType == SCAN_WIFI_STA) ? "WiFi STA" : "Unknown");
                break;
            case WendigoOptionBTCod:
                if (menu_item->scanType == SCAN_HCI || menu_item->scanType == SCAN_BLE) {
                    variable_item_set_current_value_text(item, menu_item->radio.bluetooth.cod_str);
                }
                // TODO: Other menu item(s) for WiFi devices
                break;
            case WendigoOptionBTLastSeen:
                elapsedTime(menu_item, tempStr, sizeof(tempStr));
                variable_item_set_current_value_text(item, tempStr);
                break;
            default:
                // TODO: Panic
                break;
        }
    }
}

/* Initialise the device list
   TODO: When "display options" are implemented include consideration of selected device types and sorting options
*/
void wendigo_scene_device_list_on_enter(void* context) {
    WendigoApp* app = context;
    app->current_view = WendigoAppViewDeviceList;

    variable_item_list_set_enter_callback(
        app->devices_var_item_list, wendigo_scene_device_list_var_list_enter_callback, app);

    /* Reset and re-populate the list */
    wendigo_scene_device_list_redraw(app);

    /* Restore the selected device index if it's there to restore (e.g. if we're returning from
       the device detail scene). But first test that it's in bounds, unless we've moved from all
       devices to selected only. */
    uint8_t selected_item = scene_manager_get_scene_state(app->scene_manager, WendigoSceneDeviceList);
    if (selected_item >= (display_selected_only) ? selected_devices_count : devices_count) {
        selected_item = 0;
    }
    variable_item_list_set_selected_item(app->devices_var_item_list, selected_item);
    view_dispatcher_switch_to_view(app->view_dispatcher, WendigoAppViewDeviceList);
}

bool wendigo_scene_device_list_on_event(void* context, SceneManagerEvent event) {
    WendigoApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch (event.event) {
            case Wendigo_EventListDevices:
                scene_manager_set_scene_state(
                    app->scene_manager, WendigoSceneDeviceList, app->device_list_selected_menu_index);
                scene_manager_next_scene(app->scene_manager, WendigoSceneDeviceDetail);
                break;
            default:
                // TODO: Panic
                break;
        }
        consumed = true;
    } else if(event.type == SceneManagerEventTypeTick) {
        app->device_list_selected_menu_index =
            variable_item_list_get_selected_item_index(app->devices_var_item_list);
        consumed = true;

        /* Update displayed value for any device where lastSeen is currently selected */
        wendigo_device **my_devices = (display_selected_only) ? selected_devices : devices;
        uint16_t my_devices_count = (display_selected_only) ? selected_devices_count : devices_count;
        char elapsedStr[7];
        uint32_t now = furi_hal_rtc_get_timestamp();
        for (uint16_t i = 0; i < my_devices_count; ++i) {
            if (my_devices != NULL && my_devices[i] != NULL && my_devices[i]->view != NULL &&
                    variable_item_get_current_value_index(my_devices[i]->view) == WendigoOptionBTLastSeen) {
                /* Update option text if lastSeen is selected for this device */
                _elapsedTime((uint32_t *)&(my_devices[i]->lastSeen.tv_sec), &now, elapsedStr, sizeof(elapsedStr));
                variable_item_set_current_value_text(my_devices[i]->view, elapsedStr);
            }
        }
    }
    return consumed;
}

void wendigo_scene_device_list_on_exit(void* context) {
    WendigoApp* app = context;
    variable_item_list_reset(app->devices_var_item_list);
    for (uint16_t i = 0; i < devices_count; ++i) {
        devices[i]->view = NULL;
    }
    if (current_devices != NULL) {
        free(current_devices);
    }
}
