#include "../wendigo_app_i.h"
#include "../wendigo_scan.h"

wendigo_device *current_device = NULL;

void wendigo_scene_pnl_list_set_device(wendigo_device *dev) {
    current_device = dev;
    // TODO: if (app->current_view == WendigoAppViewPNLList) redraw() - Need to get context (WendigoApp *)
}

wendigo_device *wendigo_scene_pnl_list_get_device() {
    return current_device;
}

/** Returns the number of networks the specified STA has probed for */
uint8_t count_networks_for_device(wendigo_device *dev) {
    if (dev == NULL || dev->scanType != SCAN_WIFI_STA ||
            dev->radio.sta.saved_networks_count == 0 ||
            dev->radio.sta.saved_networks == NULL) {
        return 0;
    }
    return dev->radio.sta.saved_networks_count;
}

/** Retrieve the SSIDs that the specified device has sent probes for.
 * SSIDs will be stored in ***result, which must be an UNINITIALISED pointer.
 * This function will allocate memory for the pointer array and for each
 * char[], the caller is responsible for freeing this memory when it's no
 * longer needed.
 * To be clear, char ***result means you should declare a char **variable,
 * which is an array of strings. When calling the function you use
 *              get_networks_for_device(dev, &variable);
 * That allows this function to modify the memory where your local
 * char **variable is stored - meaning I can allocate enough pointers to
 * hold the SSIDs.
 * Returns the number of SSIDs in results.
 */
uint8_t get_networks_for_device(WendigoApp *app, wendigo_device *dev, char ***result) {
    uint8_t networks = count_networks_for_device(dev);
    if (networks == 0) {
        return 0;
    }
    /* Allocate memory for the caller's char **result */
    char **res = malloc(sizeof(char *) * networks);
    *result = res;
    if (res == NULL) {
        char *msg = malloc(sizeof(char) * 48);
        if (msg == NULL) {
            wendigo_display_popup(app, "Insufficient memory", "Unable to allocate memory for preferred network list.");
            wendigo_log(MSG_ERROR, "Unable to allocate memory for device's PNL.");
        } else {
            snprintf(msg, 48, "Unable to allocate %d bytes for device's PNL.", sizeof(char) * networks);
            wendigo_log(MSG_ERROR, msg);
            wendigo_display_popup(app, "Insufficient memory", msg);
            free(msg);
        }
        return 0;
    }
    for (uint8_t i = 0; i < networks; ++i) {
        if (dev->radio.sta.saved_networks[i] == NULL ||
                strlen(dev->radio.sta.saved_networks[i]) == 0) {
            res[i] = malloc(sizeof(char) * (strlen(dev->radio.sta.saved_networks[i]) + 1));
            if (res[i] == NULL) {
                // Failed to allocate %d bytes for a probed SSID.  49
                char *msg = malloc(sizeof(char) * 55);
                if (msg == NULL) {
                    wendigo_log(MSG_ERROR, "Failed to allocate memory for a probed network.");
                } else {
                    snprintf(msg, 55, "Failed to allocate %d bytes for a probed SSID.", strlen(dev->radio.sta.saved_networks[i]) + 1);
                    wendigo_log(MSG_ERROR, msg);
                    free(msg);
                }
            } else {
                strncpy(res[i], dev->radio.sta.saved_networks[i], strlen(dev->radio.sta.saved_networks[i]) + 1);
            }
        }
    }
    return networks;
}

uint16_t get_all_networks(WendigoApp *app) {
    char **networks;
    int16_t count = 0;
    /* First find the size of our array */
    for (uint16_t i = 0; i < devices_count; ++i) {
        if (devices != NULL && devices[i] != NULL && devices[i]->scanType == SCAN_WIFI_STA) {
            count += count_networks_for_device(devices[i]);
        }
    }
    networks = malloc(sizeof(char *) * count);
    if (networks == NULL) {
        char *msg = malloc(sizeof(char) * 44);
        if (msg == NULL) {
            wendigo_log(MSG_ERROR, "Failed to allocate memory to stringify PNLs.");
        } else {
            snprintf(msg, 44, "Failed to allocatte memory for %d PNLs.", count);
            wendigo_log(MSG_ERROR, msg);
            free(msg);
        }
        return 0;
    }
    /* Call get_networks_for_device() on each device and concatenate their results */
    char **dev_nets;
    uint8_t dev_count;
    for (uint16_t i = 0; i < devices_count; ++i) {
        if (devices != NULL && devices[i] != NULL && devices[i]->scanType == SCAN_WIFI_STA) {
            dev_count = get_networks_for_device(app, devices[i], &dev_nets);
            if (dev_count > 0 && dev_nets != NULL) {
                /* Copy dev_nets into networks */
                // TODO Maintain an index into networks[].
            }
        }
    }
    return 0;
}

void wendigo_scene_pnl_list_redraw(WendigoApp *app) {
    VariableItemList *var_item_list = app->var_item_list;
    variable_item_list_reset(var_item_list);

    if (current_device == NULL || current_device->scanType != SCAN_WIFI_STA) {
        return;
    }
    VariableItem *item;
    if (current_device->radio.sta.saved_networks_count == 0 ||
            current_device->radio.sta.saved_networks == NULL) {
        item = variable_item_list_add(var_item_list, "No networks found", 1,
            NULL, app);
        variable_item_set_current_value_index(item, 0);
        variable_item_set_current_value_text(item, "");
        return;
    }
    /* Display the MAC as the list header. Shame we can't make this text smaller. */
    char macStr[MAC_STRLEN + 1];
    bytes_to_string(current_device->mac, MAC_BYTES, macStr);
    variable_item_list_set_header(var_item_list, macStr);
    for (uint8_t i = 0; i < current_device->radio.sta.saved_networks_count; ++i) {
        if (current_device->radio.sta.saved_networks[i] != NULL) {
            item = variable_item_list_add(var_item_list,
                current_device->radio.sta.saved_networks[i], 1, NULL, app);
            variable_item_set_current_value_index(item, 0);
            variable_item_set_current_value_text(item, "");
        }
    }
}

/** Scene initialisation.
 */
void wendigo_scene_pnl_list_on_enter(void *context) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_pnl_list_on_enter()");
    WendigoApp *app = context;
    app->current_view = WendigoAppViewPNLList;

    wendigo_scene_pnl_list_redraw(app);

    view_dispatcher_switch_to_view(app->view_dispatcher,
                                    WendigoAppViewVarItemList);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_pnl_list_on_enter()");
}

/** We have no need to respond to events */
bool wendigo_scene_pnl_list_on_event(void *context, SceneManagerEvent event) {
    FURI_LOG_T(WENDIGO_TAG, "Start+End wendigo_scene_pnl_list_on_event()");
    UNUSED(context);
    UNUSED(event);
    return false;
}

void wendigo_scene_pnl_list_on_exit(void *context) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_pnl_list_on_exit()");
    WendigoApp *app = context;
    variable_item_list_reset(app->var_item_list);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_pnl_list_on_exit()");
}
