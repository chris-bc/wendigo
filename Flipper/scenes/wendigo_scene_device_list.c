#include "../wendigo_app_i.h"
#include "../wendigo_scan.h"

/* Public method from wendigo_scene_device_detail.c */
extern void wendigo_scene_device_detail_set_device(flipper_bt_device *d);

/* This scene is used to both display all devices and display selected devices */
bool display_selected_only = false;

/* Enum to index the options menu for devices */
enum wendigo_device_list_options {
    WendigoOptionRSSI = 0,
    WendigoOptionTagUntag,
    WendigoOptionScanType,
    WendigoOptionCod,
    WendigoOptionLastSeen,
    WendigoOptionsCount
};

/** A more flexible version of elapsedTime() that lets us avoid running furi_hal_rtc_get_timestamp().
 * This version is suitable for running at high frequency - In tick events etc.
 * Returns elapsed seconds and, if elapstedStr is an initialised char[] and strlen > 0, places
 * a text representation of the elapsed time in elapsedStr.
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
    double elapsed = to - from;
    if (elapsedStr != NULL && strlen > 0) {
        if (elapsed < 60) {
            snprintf(elapsedStr, strlen, "%fs", elapsed);
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
 * Returns zero and an empty string on failure.
 */
double elapsedTime(flipper_bt_device *dev, char *elapsedStr, uint8_t strlen) {
    if (dev == NULL) {
        /* Return sensible values on failure */
        if (strlen > 0 && elapsedStr != NULL) {
            elapsedStr[0] = '\0';
        }
        return 0;
    }
    uint32_t nowTime = furi_hal_rtc_get_timestamp();
    return _elapsedTime((uint32_t *)&(dev->dev.lastSeen.tv_sec), &nowTime, elapsedStr, strlen);
}

static void wendigo_scene_device_list_var_list_enter_callback(void* context, uint32_t index) {
    furi_assert(context);
    WendigoApp* app = context;

    furi_assert(index < ((display_selected_only) ? bt_selected_devices_count : bt_devices_count));

    flipper_bt_device *item = (display_selected_only) ? bt_selected_devices[index] : bt_devices[index];

    app->device_list_selected_menu_index = index;

    /* If the tag/untag menu item is selected perform that action, otherwise display details for `item` */
    if (item->view != NULL && variable_item_get_current_value_index(item->view) == WendigoOptionTagUntag) {
        wendigo_set_bt_device_selected(item, !item->dev.tagged);
        variable_item_set_current_value_text(item->view, (item->dev.tagged) ? "Untag" : "Tag");
    } else {
        /* Display details for `item` */
        wendigo_scene_device_detail_set_device(item);
        view_dispatcher_send_custom_event(app->view_dispatcher, Wendigo_EventListDevices);
    }
}

static void wendigo_scene_device_list_var_list_change_callback(VariableItem* item) {
    furi_assert(item);

    WendigoApp* app = variable_item_get_context(item);
    furi_assert(app);

    flipper_bt_device *menu_item = (display_selected_only) ?
                                    bt_selected_devices[app->device_list_selected_menu_index] :
                                    bt_devices[app->device_list_selected_menu_index];
    uint8_t item_index = variable_item_get_current_value_index(item);
    furi_assert(item_index < ((display_selected_only) ? bt_selected_devices_count : bt_devices_count));
    // TODO: The following will be useful for managing tag options (on/off)
    // variable_item_set_current_value_text(item, menu_item->options_menu[item_index]);
    // app->setup_selected_option_index[app->setup_selected_menu_index] = item_index;
    
    /* Update the current option label */
    char tempStr[16];
    switch (item_index) {
        case WendigoOptionRSSI:
            snprintf(tempStr, sizeof(tempStr), "%ld dB", menu_item->dev.rssi);
            variable_item_set_current_value_text(item, tempStr);
            break;
        case WendigoOptionTagUntag:
            variable_item_set_current_value_text(item, (menu_item->dev.tagged) ? "Untag" : "Tag");
            break;
        case WendigoOptionScanType:
            variable_item_set_current_value_text(item, (menu_item->dev.scanType == SCAN_HCI) ? "BT Classic" :
                (menu_item->dev.scanType == SCAN_BLE) ? "BLE" : (menu_item->dev.scanType == SCAN_WIFI) ?
                "WiFi" : "Unknown");
            break;
        case WendigoOptionCod:
            variable_item_set_current_value_text(item, menu_item->cod_str);
            break;
        case WendigoOptionLastSeen:
            elapsedTime(menu_item, tempStr, sizeof(tempStr));
            variable_item_set_current_value_text(item, tempStr);
            break;
        default:
            // TODO: Panic
            break;
    }

}

void wendigo_scene_device_list_update(WendigoApp *app, flipper_bt_device *dev) {
    char *name;
    if (dev->dev.bdname_len == 0 || dev->dev.bdname == NULL) {
        name = malloc(sizeof(char) * (MAC_STRLEN + 1));
        if (name == NULL) {
            // TODO: Panic
            return;
        }
        bytes_to_string(dev->dev.bda, MAC_BYTES, name);
    } else {
        name = malloc(sizeof(char) * (dev->dev.bdname_len + 1));
        strncpy(name, dev->dev.bdname, dev->dev.bdname_len);
    }
    char tempStr[10];
    if (dev->view == NULL && !display_selected_only) {
        /* Add a new item if we're not displaying tagged items only */
        dev->view = variable_item_list_add(
            app->devices_var_item_list,
            name,
            WendigoOptionsCount,
            wendigo_scene_device_list_var_list_change_callback,
            app);
        variable_item_set_current_value_index(dev->view, WendigoOptionRSSI);
        snprintf(tempStr, sizeof(tempStr), "%ld dB", dev->dev.rssi);
        variable_item_set_current_value_text(dev->view, tempStr);
    } else if (dev->view != NULL) {
        /* Update dev->view */
        variable_item_set_item_label(dev->view, name);
        /* If RSSI or lastSeen is displayed, update their values */
        if (variable_item_get_current_value_index(dev->view) == WendigoOptionRSSI) {
            snprintf(tempStr, sizeof(tempStr), "%ld dB", dev->dev.rssi);
            variable_item_set_current_value_text(dev->view, tempStr);
        } else if (variable_item_get_current_value_index(dev->view) == WendigoOptionLastSeen) {
            elapsedTime(dev, tempStr, sizeof(tempStr));
            variable_item_set_current_value_text(dev->view, tempStr);
        }
    } /* Ignore new devices if display_selected_only is true */
    free(name);

    // TODO: Add/Update the options menu: display RSSI where adding, check the
    //       selected option index where updating to determine whether anything else needs to be updated
    variable_item_set_current_value_index(dev->view, WendigoOptionRSSI);
}

/* Initialise the device list
   TODO: When "display options" are implemented include consideration of selected device types and sorting options
*/
void wendigo_scene_device_list_on_enter(void* context) {
    WendigoApp* app = context;
    app->current_view = WendigoAppViewDeviceList;
    VariableItemList* var_item_list = app->devices_var_item_list;

    variable_item_list_set_enter_callback(
        var_item_list, wendigo_scene_device_list_var_list_enter_callback, app);

    variable_item_list_reset(var_item_list);
    char *item_str;
    uint16_t device_count;
    flipper_bt_device **devices;
    if (display_selected_only) {
        device_count = bt_selected_devices_count;
        devices = bt_selected_devices;
    } else {
        device_count = bt_devices_count;
        devices = bt_devices;
    }
    for(int i = 0; i < device_count; ++i) {
        /* Label with the name if we have a name, otherwise use the BDA */
        if (devices[i]->dev.bdname_len > 0 && devices[i]->dev.bdname != NULL) {
            item_str = devices[i]->dev.bdname;
        } else {
            item_str = malloc(sizeof(char) * (MAC_STRLEN + 1));
            if (item_str == NULL) {
                // TODO: Panic
                continue; // Maybe the next device will have a name so we won't need to malloc
            }
            bytes_to_string(devices[i]->dev.bda, MAC_BYTES, item_str);
        }
        devices[i]->view = variable_item_list_add(
            var_item_list,
            item_str,
            WendigoOptionsCount,
            wendigo_scene_device_list_var_list_change_callback,
            app);
        if (devices[i]->dev.bdname_len == 0 || devices[i]->dev.bdname == NULL) {
            free(item_str);
        }
        /* Default to displaying RSSI in options menu */
        char tempStr[10];
        snprintf(tempStr, sizeof(tempStr), "%ld dB", devices[i]->dev.rssi);
        variable_item_set_current_value_index(devices[i]->view, WendigoOptionRSSI);
        variable_item_set_current_value_text(devices[i]->view, tempStr);
    }
    // TODO: Display WiFi devices

    variable_item_list_set_selected_item(
        var_item_list, scene_manager_get_scene_state(app->scene_manager, WendigoSceneDeviceList));

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
        flipper_bt_device **devices = (display_selected_only) ? bt_selected_devices : bt_devices;
        uint16_t devices_count = (display_selected_only) ? bt_selected_devices_count : bt_devices_count;
        char elapsedStr[7];
        uint32_t now = furi_hal_rtc_get_timestamp();
        for (int i = 0; i < devices_count; ++i) {
            /* I can probably remove this but I'd rather be certain than have to find this as a bug later */
            if (devices != NULL && devices[i] != NULL) {
                /* Update lastSeen */
                devices[i]->dev.lastSeen.tv_sec = (time_t)now; /* casting to a long long int is OK */
                /* Update option text if lastSeen is selected for this device */
                if (devices[i]->view != NULL &&
                        variable_item_get_current_value_index(devices[i]->view) == WendigoOptionLastSeen) {
                    _elapsedTime((uint32_t *)&(devices[i]->dev.lastSeen.tv_sec), &now, elapsedStr, sizeof(elapsedStr));
                    variable_item_set_current_value_text(devices[i]->view, elapsedStr);
                    UNUSED(elapsedStr);
                }
            } /* Maybe the next device won't be empty */
        }
    }
    return consumed;
}

void wendigo_scene_device_list_on_exit(void* context) {
    WendigoApp* app = context;
    variable_item_list_reset(app->devices_var_item_list);
    for (int i = 0; i < bt_devices_count; ++i) {
        bt_devices[i]->view = NULL;
    }
}
