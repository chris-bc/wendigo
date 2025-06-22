#include "../wendigo_scan.h"

/* Public method from wendigo_scene_device_detail.c */
extern void wendigo_scene_device_detail_set_device(wendigo_device *d);
/* Internal method - I don't wan't to move all calling functions below it */
static void wendigo_scene_device_list_var_list_change_callback(VariableItem* item);

/* For some obscene reason the ifndef barrier isn't stopping these from showing up in every single object file.
   No longer shared. */
char *authModeStrings[] = {"Open", "WEP", "WPA_PSK", "WPA2_PSK", "WPA_WPA2_PSK", "EAP (Enterprise)", "WPA3_PSK",
                           "WPA2_WPA3_PSK", "WAPI_PSK", "OWE", "WPA_ENT_SUITE_B_192_BIT", "WPA3_PSK", "WPA3_PSK",
                           "DPP", "WPA3-Enterprise", "WPA3-Enterprise Transition"};

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

/* Devices currently being displayed */
wendigo_device **current_devices = NULL;
uint16_t current_devices_count = 0;
uint8_t current_devices_mask = DEVICE_ALL;

/** Determine whether the specified device should be displayed, based on the criteria provided in wendigo_set_current_devices() */
bool wendigo_device_is_displayed(wendigo_device *dev) {
    bool display_selected = ((current_devices_mask & DEVICE_SELECTED_ONLY) == DEVICE_SELECTED_ONLY);
    return ((dev->scanType == SCAN_HCI && ((current_devices_mask & DEVICE_BT_CLASSIC) == DEVICE_BT_CLASSIC) && (!display_selected || dev->tagged)) ||
            (dev->scanType == SCAN_BLE && ((current_devices_mask & DEVICE_BT_LE) == DEVICE_BT_LE) && (!display_selected || dev->tagged)) ||
            (dev->scanType == SCAN_WIFI_AP && ((current_devices_mask & DEVICE_WIFI_AP) == DEVICE_WIFI_AP) && (!display_selected || dev->tagged)) ||
            (dev->scanType == SCAN_WIFI_STA && ((current_devices_mask & DEVICE_WIFI_STA) == DEVICE_WIFI_STA) && (!display_selected || dev->tagged)));
}

/** Restrict displayed devices based on the specified filters
 * deviceMask: A bitmask of DeviceMask values. e.g. DEVICE_WIFI_AP | DEVICE_WIFI_STA to display all WiFi devices;
 *             0 to display all device types.
 * Returns the number of devices that meet the specified criteria.
 */
uint16_t wendigo_set_current_devices(uint8_t deviceMask) {
    if (deviceMask == 0) {
        deviceMask = DEVICE_ALL;
    }
    bool display_selected = ((deviceMask & DEVICE_SELECTED_ONLY) == DEVICE_SELECTED_ONLY);
    /* To ensure we only malloc the required memory, run an initial pass to count the number of devices */
    uint16_t deviceCount = 0;
    for (uint16_t idx = 0; idx < devices_count; ++idx) {
        if ((devices[idx]->scanType == SCAN_HCI && ((deviceMask & DEVICE_BT_CLASSIC) == DEVICE_BT_CLASSIC) && (!display_selected || devices[idx]->tagged)) ||
            (devices[idx]->scanType == SCAN_BLE && ((deviceMask & DEVICE_BT_LE) == DEVICE_BT_LE) && (!display_selected || devices[idx]->tagged)) ||
            (devices[idx]->scanType == SCAN_WIFI_AP && ((deviceMask & DEVICE_WIFI_AP) == DEVICE_WIFI_AP) && (!display_selected || devices[idx]->tagged)) ||
            (devices[idx]->scanType == SCAN_WIFI_STA && ((deviceMask & DEVICE_WIFI_STA) == DEVICE_WIFI_STA) && (!display_selected || devices[idx]->tagged))) {
                ++deviceCount;
            }
    }
    wendigo_device **new_devices = realloc(current_devices, sizeof(wendigo_device *) * deviceCount);
    if (new_devices == NULL) {
        return 0;
    }
    current_devices = new_devices;
    current_devices_count = deviceCount;
    current_devices_mask = deviceMask;
    /* Populate current_devices[] */
    uint16_t current_index = 0;
    for (uint16_t i = 0; i < devices_count; ++i) {
        if ((devices[i]->scanType == SCAN_HCI && ((deviceMask & DEVICE_BT_CLASSIC) == DEVICE_BT_CLASSIC) && (!display_selected || devices[i]->tagged)) ||
            (devices[i]->scanType == SCAN_BLE && ((deviceMask & DEVICE_BT_LE) == DEVICE_BT_LE) && (!display_selected || devices[i]->tagged)) ||
            (devices[i]->scanType == SCAN_WIFI_AP && ((deviceMask & DEVICE_WIFI_AP) == DEVICE_WIFI_AP) && (!display_selected || devices[i]->tagged)) ||
            (devices[i]->scanType == SCAN_WIFI_STA && ((deviceMask & DEVICE_WIFI_STA) == DEVICE_WIFI_STA) && (!display_selected || devices[i]->tagged))) {
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
    
    if (app->device_list_selected_menu_index < current_devices_count) {
        return current_devices[app->device_list_selected_menu_index];
    }
// TODO: Compare these methods - I'm tempted to remove the first approach (which is why it now gets its own function)
    /* Instead of indexing into the array, see if we can find the device with a reverse lookup from `item` */
    uint16_t idx = 0;
    for (; idx < current_devices_count && current_devices[idx]->view != item; ++idx) { }
    if (idx < current_devices_count) {
        return current_devices[idx];
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
    if (!wendigo_device_is_displayed(dev)) {
        return;
    }
    char *name;
    uint8_t optionsCount;
    uint8_t optionIndex;
    char optionValue[MAX_SSID_LEN + 1];
    bzero(optionValue, sizeof(optionValue)); /* Null out optionValue[] */

    if (current_devices == NULL || current_devices_count == 0) {
        current_devices_count = wendigo_set_current_devices(current_devices_mask);
    }
    /* Set name and options menus based on dev->scanType */
    if (dev->scanType == SCAN_HCI || dev->scanType == SCAN_BLE) {
        /* Use bdname as name if it's a bluetooth device and we have a name */
        if (dev->radio.bluetooth.bdname_len > 0 && dev->radio.bluetooth.bdname != NULL) {
            name = dev->radio.bluetooth.bdname;
        } else {
            /* Otherwise use MAC */
            name = malloc(sizeof(char) * (MAC_STRLEN + 1));
            if (name != NULL) {
                bytes_to_string(dev->mac, MAC_BYTES, name);
            }
        }
        optionsCount = WendigoOptionsBTCount;
        if (dev->view == NULL) {
            optionIndex = WendigoOptionBTRSSI;
        } else {
            optionIndex = variable_item_get_current_value_index(dev->view);
        }
        switch (optionIndex) {
            case WendigoOptionBTRSSI:
                snprintf(optionValue, sizeof(optionValue), "%d dB", dev->rssi);
                break;
            case WendigoOptionBTLastSeen:
                elapsedTime(dev, optionValue, sizeof(optionValue));
                break;
            default:
                /* I'm pretty sure I don't need to worry about static variables */
                break;
        }
    } else if (dev->scanType == SCAN_WIFI_AP) {
        if (dev->radio.ap.ssid[0] == '\0') {
            /* No SSID - Use MAC */
            name = malloc(sizeof(char) * (MAC_STRLEN + 1));
            if (name != NULL) {
                bytes_to_string(dev->mac, MAC_BYTES, name);
            }
        } else {
            /* Use access point SSID as name */
            name = (char *)dev->radio.ap.ssid;
        }
        optionsCount = WendigoOptionsAPCount;
        if (dev->view == NULL) {
            optionIndex = WendigoOptionAPRSSI;
        } else {
            optionIndex = variable_item_get_current_value_index(dev->view);
        }
        switch (optionIndex) {
            case WendigoOptionAPRSSI:
                snprintf(optionValue, sizeof(optionValue), "%d dB", dev->rssi);
                break;
            case WendigoOptionAPLastSeen:
                elapsedTime(dev, optionValue, sizeof(optionValue));
                break;
            case WendigoOptionAPChannel:
                snprintf(optionValue, sizeof(optionValue), "Ch. %d", dev->radio.ap.channel);
                break;
            case WendigoOptionAPAuthMode:
                snprintf(optionValue, sizeof(optionValue), "%s", authModeStrings[dev->radio.ap.authmode]);
                break;
            default:
                // Nothing
                break;
        }
    } else if (dev->scanType == SCAN_WIFI_STA) {
        /* Use MAC to label the device */
        name = malloc(sizeof(char) * (MAC_STRLEN + 1));
        if (name != NULL) {
            bytes_to_string(dev->mac, MAC_BYTES, name);
        }
        optionsCount = WendigoOptionsSTACount;
        if (dev->view == NULL) {
            optionIndex = WendigoOptionSTARSSI;
        } else {
            optionIndex = variable_item_get_current_value_index(dev->view);
        }
        switch (optionIndex) {
            case WendigoOptionSTARSSI:
                snprintf(optionValue, sizeof(optionValue), "%d dB", dev->rssi);
                break;
            case WendigoOptionSTALastSeen:
                elapsedTime(dev, optionValue, sizeof(optionValue));
                break;
            case WendigoOptionSTAChannel:
                snprintf(optionValue, sizeof(optionValue), "Ch. %d", dev->radio.sta.channel);
                break;
            case WendigoOptionSTAAP:
                if (dev->radio.sta.ap == NULL) {
                    snprintf(optionValue, sizeof(optionValue), "Unknown");
                } else {
                    if (((wendigo_device *)dev->radio.sta.ap)->radio.ap.ssid[0] == '\0') {
                        /* No SSID for the AP - Display MAC instead */
                        bytes_to_string(dev->radio.sta.apMac, MAC_BYTES, optionValue);
                    } else {
                        /* Use AP's SSID */
                        snprintf(optionValue, sizeof(optionValue), "%s", ((wendigo_device *)dev->radio.sta.ap)->radio.ap.ssid);
                    }
                }
                break;
            default:
                /* Nothing to do, I think */
                break;
        }
    } else {
        /* Invalid device type */
        return;
    }

    if (dev->view == NULL) {
        /* Add a new item */
        dev->view = variable_item_list_add(
            app->devices_var_item_list,
            (name == NULL) ? "" : name,
            optionsCount,
            wendigo_scene_device_list_var_list_change_callback,
            app);
        /* Update current_devices[] to include the new device */
        wendigo_set_current_devices(current_devices_mask);
    } else {
        /* Update dev->view */
        variable_item_set_item_label(dev->view, (name == NULL) ? "" : name);
    }
    variable_item_set_current_value_index(dev->view, optionIndex);
    if (optionValue[0] != '\0') {
        /* New value for the option */
        variable_item_set_current_value_text(dev->view, optionValue);
    }
    /* Free `name` if we malloc'd it */
    if (((dev->scanType == SCAN_HCI || dev->scanType == SCAN_BLE) &&
                (dev->radio.bluetooth.bdname_len == 0 || dev->radio.bluetooth.bdname == NULL)) ||
            (dev->scanType == SCAN_WIFI_AP && dev->radio.ap.ssid[0] == '\0') ||
            dev->scanType == SCAN_WIFI_STA) {
        free(name);    
    }
}

/** Re-render the variable item list. This function exists because there is no method to remove
 * items from a variable_item_list, but that is sometimes necessary (e.g. when viewing selected
 * devices and de-selecting a device).
 */
void wendigo_scene_device_list_redraw(WendigoApp *app) {
    variable_item_list_reset(app->devices_var_item_list);
    char *item_str = NULL;
    uint8_t options_count = 0;
    uint8_t options_index;
    bool free_item_str = false;
    wendigo_set_current_devices(current_devices_mask);
    for(uint16_t i = 0; i < current_devices_count; ++i) {
        /* Label with bdname if it's bluetooth & we have a name */
        if (current_devices[i] != NULL && (current_devices[i]->scanType == SCAN_HCI || current_devices[i]->scanType == SCAN_BLE)) {
            if (current_devices[i]->radio.bluetooth.bdname_len > 0 && current_devices[i]->radio.bluetooth.bdname != NULL) {
                item_str = current_devices[i]->radio.bluetooth.bdname;
            } else {
                /* Use MAC */
                item_str = malloc(sizeof(char) * (MAC_STRLEN + 1));
                if (item_str != NULL) {
                    bytes_to_string(current_devices[i]->mac, MAC_BYTES, item_str);
                }
                free_item_str = true;
            }
            options_count = WendigoOptionsBTCount;
            options_index = WendigoOptionBTRSSI;
        /* Label with SSID if it's an AP and we have an SSID */
        } else if (current_devices[i] != NULL && current_devices[i]->scanType == SCAN_WIFI_AP) {
            if (current_devices[i]->radio.ap.ssid[0] != '\0') {
                item_str = (char *)current_devices[i]->radio.ap.ssid;
            } else {
                /* Use MAC */
                item_str = malloc(sizeof(char) * (MAC_STRLEN + 1));
                if (item_str != NULL) {
                    bytes_to_string(current_devices[i]->mac, MAC_BYTES, item_str);
                }
                free_item_str = true;
            }
            options_count = WendigoOptionsAPCount;
            options_index = WendigoOptionAPRSSI;
        /* Otherwise use the MAC/BDA */
        } else if (current_devices[i] != NULL) {
            item_str = malloc(sizeof(char) * (MAC_STRLEN + 1));
            if (item_str != NULL) {
                bytes_to_string(current_devices[i]->mac, MAC_BYTES, item_str);
            }
            free_item_str = true;
            options_count = WendigoOptionsSTACount;
            options_index = WendigoOptionSTARSSI;
        }
        if (current_devices[i] != NULL && item_str != NULL && options_count > 0) {
            current_devices[i]->view = variable_item_list_add(
                app->devices_var_item_list,
                item_str,
                options_count,
                wendigo_scene_device_list_var_list_change_callback,
                app);
        }
        /* free item_str if we malloc'd it */
        if (free_item_str) {
            free(item_str);
        }
        /* Default to displaying RSSI in options menu */
        if (current_devices[i] != NULL && current_devices[i]->view != NULL) {
            char tempStr[10];
            snprintf(tempStr, sizeof(tempStr), "%d dB", current_devices[i]->rssi);
            variable_item_set_current_value_index(current_devices[i]->view, options_index);
            variable_item_set_current_value_text(current_devices[i]->view, tempStr);
        }
    }
    variable_item_list_set_selected_item(app->devices_var_item_list, 0);
}

static void wendigo_scene_device_list_var_list_enter_callback(void* context, uint32_t index) {
    furi_assert(context);
    WendigoApp* app = context;

    furi_assert(index < current_devices_count);

    wendigo_device *item = current_devices[index];
    if (item == NULL) {
        return;
    }

    app->device_list_selected_menu_index = index;

    /* If the tag/untag menu item is selected perform that action, otherwise display details for `item` */
    if (item->view != NULL && (((item->scanType == SCAN_HCI || item->scanType == SCAN_BLE) &&
            variable_item_get_current_value_index(item->view) == WendigoOptionBTTagUntag) ||
            (item->scanType == SCAN_WIFI_AP && variable_item_get_current_value_index(item->view) == WendigoOptionAPTagUntag) ||
            (item->scanType == SCAN_WIFI_STA && variable_item_get_current_value_index(item->view) == WendigoOptionSTATagUntag))) {
        wendigo_set_device_selected(item, !item->tagged);
        variable_item_set_current_value_text(item->view, (item->tagged) ? "Untag" : "Tag");
        /* If the device is now untagged and we're viewing tagged devices only, remove the device from view */
        if (((current_devices_mask & DEVICE_SELECTED_ONLY) == DEVICE_SELECTED_ONLY) && !item->tagged) {
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
    WendigoApp *app = variable_item_get_context(item);
    app->device_list_selected_menu_index = variable_item_list_get_selected_item_index(app->devices_var_item_list);
    wendigo_device *menu_item = wendigo_scene_device_list_selected_device(item);

    if (menu_item != NULL) {
        uint8_t option_index = variable_item_get_current_value_index(item);
        furi_assert(option_index < current_devices_count);
        /* Update the current option label */
        char tempStr[MAX_SSID_LEN + 1];
        if (((menu_item->scanType == SCAN_HCI || menu_item->scanType == SCAN_BLE) && option_index == WendigoOptionBTRSSI) ||
                (menu_item->scanType == SCAN_WIFI_AP && option_index == WendigoOptionAPRSSI) ||
                (menu_item->scanType == SCAN_WIFI_STA && option_index == WendigoOptionSTARSSI)) {
            snprintf(tempStr, sizeof(tempStr), "%d dB", menu_item->rssi);
            variable_item_set_current_value_text(item, tempStr);
        } else if (((menu_item->scanType == SCAN_HCI || menu_item->scanType == SCAN_BLE) && option_index == WendigoOptionBTTagUntag) ||
                (menu_item->scanType == SCAN_WIFI_AP && option_index == WendigoOptionAPTagUntag) ||
                (menu_item->scanType == SCAN_WIFI_STA && option_index == WendigoOptionSTATagUntag)) {
            variable_item_set_current_value_text(item, (menu_item)->tagged ? "Untag" : "Tag");
        } else if (((menu_item->scanType == SCAN_HCI || menu_item->scanType == SCAN_BLE) && option_index == WendigoOptionBTScanType) ||
                (menu_item->scanType == SCAN_WIFI_AP && option_index == WendigoOptionAPScanType) ||
                (menu_item->scanType == SCAN_WIFI_STA && option_index == WendigoOptionSTAScanType)) {
            variable_item_set_current_value_text(item, (menu_item->scanType == SCAN_HCI) ? "BT Classic" :
                    (menu_item->scanType == SCAN_BLE) ? "BLE" : (menu_item->scanType == SCAN_WIFI_AP) ?
                    "WiFi AP" : (menu_item->scanType == SCAN_WIFI_STA) ? "WiFi STA" : "Unknown");
        } else if (((menu_item->scanType == SCAN_HCI || menu_item->scanType == SCAN_BLE) && option_index == WendigoOptionBTLastSeen) ||
                (menu_item->scanType == SCAN_WIFI_AP && option_index == WendigoOptionAPLastSeen) ||
                (menu_item->scanType == SCAN_WIFI_STA && option_index == WendigoOptionSTALastSeen)) {
            elapsedTime(menu_item, tempStr, sizeof(tempStr));
            variable_item_set_current_value_text(item, tempStr);
        } else if ((menu_item->scanType == SCAN_HCI || menu_item->scanType == SCAN_BLE) && option_index == WendigoOptionBTCod) {
            variable_item_set_current_value_text(item, menu_item->radio.bluetooth.cod_str);
        } else if (menu_item->scanType == SCAN_WIFI_AP && option_index == WendigoOptionAPChannel) {
            snprintf(tempStr, sizeof(tempStr), "Ch. %d", menu_item->radio.ap.channel);
            variable_item_set_current_value_text(item, tempStr);
        } else if (menu_item->scanType == SCAN_WIFI_STA && option_index == WendigoOptionSTAChannel) {
            snprintf(tempStr, sizeof(tempStr), "Ch. %d", menu_item->radio.sta.channel);
            variable_item_set_current_value_text(item, tempStr);
        } else if (menu_item->scanType == SCAN_WIFI_AP && option_index == WendigoOptionAPAuthMode) {
            snprintf(tempStr, sizeof(tempStr), "%s", authModeStrings[menu_item->radio.ap.authmode]);
            variable_item_set_current_value_text(item, tempStr);
        } else if (menu_item->scanType == SCAN_WIFI_STA && option_index == WendigoOptionSTAAP) {
            /* Use SSID if available, otherwise MAC */
            if (menu_item->radio.sta.ap == NULL || ((wendigo_device *)menu_item->radio.sta.ap)->radio.ap.ssid[0] == '\0') {
                if (menu_item->radio.sta.apMac[0] == '\0') {
                    /* No MAC - Empty string */
                    tempStr[0] = '\0';
                } else {
                    /* We have a MAC */
                    bytes_to_string(menu_item->radio.sta.apMac, MAC_BYTES, tempStr);
                }
            } else {
                /* We have an SSID */
                snprintf(tempStr, sizeof(tempStr), "%s", (char *)((wendigo_device *)menu_item->radio.sta.ap)->radio.ap.ssid);
            }
            variable_item_set_current_value_text(item, tempStr);
        } // TODO: Else panic

    }
}

/* Initialise the device list
   TODO: When "display options" are implemented include consideration of selected device types and sorting options
*/
void wendigo_scene_device_list_on_enter(void* context) {
    WendigoApp* app = context;
    app->current_view = WendigoAppViewDeviceList;

    current_devices_count = wendigo_set_current_devices(current_devices_mask);
    
    variable_item_list_set_enter_callback(
        app->devices_var_item_list, wendigo_scene_device_list_var_list_enter_callback, app);

    /* Reset and re-populate the list */
    wendigo_scene_device_list_redraw(app);

    /* Restore the selected device index if it's there to restore (e.g. if we're returning from
       the device detail scene). But first test that it's in bounds, unless we've moved from all
       devices to a subset. */
    uint8_t selected_item = scene_manager_get_scene_state(app->scene_manager, WendigoSceneDeviceList);
    if (selected_item >= current_devices_count) {
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
        char elapsedStr[7];
        uint32_t now = furi_hal_rtc_get_timestamp();
        for (uint16_t i = 0; i < current_devices_count; ++i) {
            if (current_devices != NULL && current_devices[i] != NULL && current_devices[i]->view != NULL &&
                    (((current_devices[i]->scanType == SCAN_HCI || current_devices[i]->scanType == SCAN_BLE) &&
                    variable_item_get_current_value_index(current_devices[i]->view) == WendigoOptionBTLastSeen) ||
                    (current_devices[i]->scanType == SCAN_WIFI_AP && variable_item_get_current_value_index(current_devices[i]->view) == WendigoOptionAPLastSeen) ||
                    (current_devices[i]->scanType == SCAN_WIFI_STA && variable_item_get_current_value_index(current_devices[i]->view) == WendigoOptionSTALastSeen))) {
                /* Update option text if lastSeen is selected for this device */
                _elapsedTime((uint32_t *)&(current_devices[i]->lastSeen.tv_sec), &now, elapsedStr, sizeof(elapsedStr));
                variable_item_set_current_value_text(current_devices[i]->view, elapsedStr);
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
        current_devices = NULL;
        current_devices_count = 0;
    }
}
