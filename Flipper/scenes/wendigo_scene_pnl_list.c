#include "../wendigo_app_i.h"
#include "../wendigo_scan.h"

/** PreferredNetwork is used to allow us to browse from SSID to devices
 * that have probed for that SSID.
 */
typedef struct PreferredNetwork {
    char ssid[MAX_SSID_LEN + 1];
    uint8_t device_count;
    wendigo_device **devices;
} PreferredNetwork;

wendigo_device *current_device = NULL;
PreferredNetwork *networks = NULL;
uint8_t networks_count = 0;

/** When displaying all probed networks this function handles the display of
 * a device list when an SSID is selected.
 */
static void wendigo_scene_pnl_list_var_list_enter_callback(void *context, uint32_t index) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scen_pnl_list_var_list_enter_callback()");
    /* If we're displaying PNL for a specific device this function has no effect */
    if (current_device != NULL && current_device->scanType == SCAN_WIFI_STA) {
        FURI_LOG_T(WENDIGO_TAG, "End wendigo_scen_pnl_list_var_list_enter_callback() - current_device is valid.");
        return;
    }
    WendigoApp *app = (WendigoApp *)context;
    if (index >= networks_count) {
        /* Index out of bounds - Panic and quit */
        char *msg = malloc(sizeof(char) * 115);
        if (msg == NULL) {
            wendigo_log(MSG_ERROR, "wendigo_scene_pnl_list_var_list_enter_callback() called with index out of bounds.");
            wendigo_display_popup(app, "Index out of bounds",
                "wendigo_scene_pnl_list_var_list_enter_callback() called with index out of bounds.");
        } else {
            snprintf(msg, 115,
                "wendigo_scene_pnl_list_var_list_enter_callback(): Called with index %ld which is out of bounds of networks[%d].",
                index, networks_count);
            wendigo_log(MSG_ERROR, msg);
            wendigo_display_popup(app, "Index out of bounds", msg);
            free(msg);
        }
        FURI_LOG_T(WENDIGO_TAG, "End wendigo_scen_pnl_list_var_list_enter_callback() - index out of bounds.");
        return;
    }
    /* Display a device list for networks[index].devices */
    DeviceListInstance deviceList;
    bzero(&deviceList, sizeof(DeviceListInstance));
    strncpy(deviceList.devices_msg, networks[index].ssid, MAX_SSID_LEN);
    deviceList.devices_mask = DEVICE_CUSTOM;
    deviceList.free_devices = false;
    deviceList.view = WendigoAppViewPNLDeviceList;
    deviceList.devices = networks[index].devices;
    deviceList.devices_count = networks[index].device_count;
    wendigo_scene_device_list_set_current_devices(&deviceList);
    /* Save selected menu item index so it can be restored later */
    scene_manager_set_scene_state(app->scene_manager, WendigoScenePNLList, index);
    scene_manager_next_scene(app->scene_manager, WendigoSceneDeviceList);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scen_pnl_list_var_list_enter_callback()");
}

/** Search networks[] for a PreferredNetwork with the specified SSID.
 * Returns the index of the PreferredNetwork, or networks_count if not
 * found.
 * SSID must be a null-terminated string.
 */
uint8_t index_of_pnl(char *ssid) {
    FURI_LOG_T(WENDIGO_TAG, "Start index_of_pnl()");
    if (ssid == NULL || strlen(ssid) == 0 || networks == NULL || networks_count == 0) {
        FURI_LOG_T(WENDIGO_TAG, "End index_of_pnl() - Invalid arguments.");
        return 0;
    }
    uint8_t idx;
    uint8_t compareLen = MAX_SSID_LEN;
    if (strlen(ssid) < compareLen) {
        compareLen = strlen(ssid);
    }
    for (idx = 0; idx < networks_count && strncmp(ssid, networks[idx].ssid, compareLen); ++idx) { }
    FURI_LOG_T(WENDIGO_TAG, "End index_of_pnl()");
    return idx;
}

/** Find the PreferredNetwork element of networks[] containing the
 * specified SSID. Returns a reference to the element of networks[]
 * or NULL if not found;
 * SSID must be a null-terminated string.
 */
PreferredNetwork *pnl_for_ssid(char *ssid) {
    FURI_LOG_T(WENDIGO_TAG, "Start pnl_for_ssid()");
    PreferredNetwork *result = NULL;
    uint8_t idx = index_of_pnl(ssid);
    if (idx < networks_count) {
        result = &(networks[idx]);
    }
    FURI_LOG_T(WENDIGO_TAG, "End pnl_for_ssid()");
    return result;
}

/** Returns the number of networks the specified STA has probed for */
uint8_t count_networks_for_device(wendigo_device *dev) {
    FURI_LOG_T(WENDIGO_TAG, "Start count_networks_for_device()");
    if (dev == NULL || dev->scanType != SCAN_WIFI_STA ||
            dev->radio.sta.saved_networks_count == 0 ||
            dev->radio.sta.saved_networks == NULL) {
        FURI_LOG_T(WENDIGO_TAG, "End count_networks_for_device() - No networks.");
        return 0;
    }
    FURI_LOG_T(WENDIGO_TAG, "End count_networks_for_device()");
    return dev->radio.sta.saved_networks_count;
}

/** Process the device cache (devices[]) and generate PreferredNetwork instances
 * that map all identified networks to the wendigo_device instances that have
 * probed for them.
 * networks[] and networks_count are updated by this function.
 */
uint8_t map_ssids_to_devices(WendigoApp *app) {
    FURI_LOG_T(WENDIGO_TAG, "Start map_ssids_to_devices()");
    if (networks_count > 0 && networks != NULL) {
        // TODO: Refactor function to update, rather than replace, networks[]
        free(networks);
        networks = NULL;
        networks_count = 0;
    }
    // TODO: Consider whether a mutex is needed over networks[]

    uint8_t networks_capacity = 0;
    uint8_t this_count;
    PreferredNetwork *new_networks;
    /* Loop over each device */
    for (uint16_t i = 0; i < devices_count; ++i) {
        if (devices == NULL || devices[i] == NULL || devices[i]->scanType != SCAN_WIFI_STA ||
                devices[i]->radio.sta.saved_networks == NULL) {
            this_count = 0;
        } else {
            this_count = devices[i]->radio.sta.saved_networks_count;
        }
        /* Make sure networks[] is large enough to include this_count additional nets */
        if (networks_count + this_count > networks_capacity) {
            new_networks = realloc(networks, sizeof(PreferredNetwork) * (networks_count + this_count));
            if (new_networks == NULL) {
                /* Too annoying to continue on - log and alert error, then clean up and exit */
                wendigo_log(MSG_ERROR, "Unable to allocate PreferredNetwork elements.");
                wendigo_display_popup(app, "Insufficient memory", "Unable to allocate PreferredNetwork elements.");
                if (networks_capacity > 0) {
                    // TODO: Better NULL checking
                    free(networks);
                    networks = NULL;
                    networks_count = 0;
                }
                FURI_LOG_T(WENDIGO_TAG, "End map_ssids_to_devices() - Unable to resize networks[].");
                return 0;
            }
            /* Zero out newly-allocated space to initialise new PreferredNetwork elements
             * (NULL ssid[] and devices[], 0 device_count). */
            bzero(new_networks + (networks_count * sizeof(PreferredNetwork)), this_count * sizeof(PreferredNetwork));
            networks_capacity = networks_count + this_count;
            networks = new_networks;
        }
        /* Check each saved network for devices[i] */
        for (uint8_t j = 0; j < this_count; ++j) {
            new_networks = pnl_for_ssid(devices[i]->radio.sta.saved_networks[j]);
            if (new_networks == NULL) {
                strncpy(networks[networks_count].ssid, devices[i]->radio.sta.saved_networks[j],
                    strlen(devices[i]->radio.sta.saved_networks[j]));
                // TODO: Once bugs are addressed, test to confirm that the following 2 lines can be removed
                networks[networks_count].device_count = 0;
                networks[networks_count].devices = NULL;
                new_networks = &(networks[networks_count++]);
            }
            /* Is devices[i] in new_networks->devices? */
            uint8_t devIdx;
            // TODO: Should I just compare pointer addresses instead of MAC?
            for (devIdx = 0; devIdx < new_networks->device_count &&
                (new_networks->devices[devIdx] == NULL ||
                    memcmp(new_networks->devices[devIdx]->mac, devices[i]->mac, MAC_BYTES));
                ++devIdx) { }
            if (devIdx == new_networks->device_count) {
                /* Append devices[i] to new_networks->devices */
                wendigo_device **new_devices = realloc(new_networks->devices,
                    sizeof(wendigo_device *) * (new_networks->device_count + 1));
                if (new_devices == NULL) {
                    char *msg = malloc(sizeof(char) * (85 + MAX_SSID_LEN + MAC_STRLEN));
                    if (msg == NULL) {
                        wendigo_log(MSG_ERROR, "Unable to allocate memory to store an additional device in a PreferredNetwork.");
                    } else {
                        char *staMac = malloc(sizeof(char) * (MAC_STRLEN + 1));
                        if (staMac == NULL) {
                            snprintf(msg, 85 + MAX_SSID_LEN + MAC_STRLEN,
                                "Unable to allocate an additional %d bytes to %s's PreferredNetwork element.",
                                sizeof(wendigo_device *), new_networks->ssid);
                        } else {
                            bytes_to_string(devices[i]->mac, MAC_BYTES, staMac);
                            snprintf(msg, 85 + MAX_SSID_LEN + MAC_STRLEN,
                                "Unable to allocate an additional %d bytes to %s's PreferredNetwork element for STA %s.",
                                sizeof(wendigo_device *), new_networks->ssid, staMac);
                            free(staMac);
                        }
                        wendigo_log(MSG_ERROR, msg);
                        free(msg);
                    }
                } else {
                    /* Append devices[i] */
                    new_devices[new_networks->device_count++] = devices[i];
                    new_networks->devices = new_devices;
                }
            }
        }
    }
    if (networks_capacity > networks_count) {
        new_networks = realloc(networks, sizeof(PreferredNetwork) * networks_count);
        if (new_networks == NULL) {
            /* This condition will (should) never be true */
            wendigo_log(MSG_ERROR, "Unable to shrink networks[] to remove spare capacity.");
            /* Don't fret too much */
        } else {
            networks = new_networks;
        }
    }
    FURI_LOG_T(WENDIGO_TAG, "End map_ssids_to_devices()");
    return networks_count;
}

void wendigo_scene_pnl_list_redraw_sta(WendigoApp *app) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_pnl_list_redraw_sta()");
    VariableItemList *var_item_list = app->var_item_list;

    if (current_device == NULL || current_device->scanType != SCAN_WIFI_STA) {
        FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_pnl_list_redraw_sta() - Invalid current_device.");
        return;
    }
    VariableItem *item;
    if (current_device->radio.sta.saved_networks_count == 0 ||
            current_device->radio.sta.saved_networks == NULL) {
        item = variable_item_list_add(var_item_list, "No networks found", 1,
            NULL, app);
        variable_item_set_current_value_index(item, 0);
        variable_item_set_current_value_text(item, "");
        FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_pnl_list_redraw_sta() - No networks to display.");
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
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_pnl_list_redraw_sta()");
}

void wendigo_scene_pnl_list_redraw_all_devices(WendigoApp *app) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_pnl_list_redraw_all_devices()");
    /* Initialise or update networks[] prior to display */
    map_ssids_to_devices(app);
    VariableItem *item;
    if (networks_count == 0 || networks == NULL) {
        item = variable_item_list_add(app->var_item_list, "No networks found",
            1, NULL, app);
        variable_item_set_current_value_index(item, 0);
        variable_item_set_current_value_text(item, "");
        FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_pnl_list_redraw_all_devices() - No networks to display.");
        return;
    }
    /* Set the list header */
    variable_item_list_set_header(app->var_item_list, "Probed Networks");
    
    /* Display networks[] */
    char countStr[9];
    for (uint8_t idx = 0; idx < networks_count; ++idx) {
        /* Add networks[idx].ssid with option networks[idx].device_count */
        snprintf(countStr, sizeof(countStr), "%d STA%s", networks[idx].device_count,
            (networks[idx].device_count == 1) ? "" : "s");
        item = variable_item_list_add(app->var_item_list, networks[idx].ssid,
            1, NULL, app);
        variable_item_set_current_value_index(item, 0);
        variable_item_set_current_value_text(item, countStr);
    }
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_pnl_list_redraw_all_devices()");
}

void wendigo_scene_pnl_list_redraw(WendigoApp *app) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_pnl_list_redraw()");
    if (current_device != NULL && current_device->scanType == SCAN_WIFI_STA) {
        FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_pnl_list_redraw() - Displaying STA.");
        return wendigo_scene_pnl_list_redraw_sta(app);
    }
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_pnl_list_redraw() - Displaying all devices.");
    return wendigo_scene_pnl_list_redraw_all_devices(app);
}

void wendigo_scene_pnl_list_set_device(wendigo_device *dev, WendigoApp *app) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_pnl_list_set_device()");
    current_device = dev;
    /* Redraw the list if this scene is active */
    // if (app->current_view == WendigoAppViewPNLList) {
    //     FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_pnl_list_set_device() - Redrawing.");
    //     return wendigo_scene_pnl_list_redraw(app);
    // }
    UNUSED(app);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_pnl_list_set_device()");
}

wendigo_device *wendigo_scene_pnl_list_get_device() {
    FURI_LOG_T(WENDIGO_TAG, "Start+End wendigo_scene_pnl_list_get_device()");
    return current_device;
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
    FURI_LOG_T(WENDIGO_TAG, "Start get_networks_for_device()");
    uint8_t nets_count = count_networks_for_device(dev);
    if (nets_count == 0) {
        FURI_LOG_T(WENDIGO_TAG, "End get_networks_for_device() - No networks.");
        return 0;
    }
    /* Allocate memory for the caller's char **result */
    char **res = malloc(sizeof(char *) * nets_count);
    *result = res;
    if (res == NULL) {
        char *msg = malloc(sizeof(char) * 48);
        if (msg == NULL) {
            wendigo_display_popup(app, "Insufficient memory", "Unable to allocate memory for preferred network list.");
            wendigo_log(MSG_ERROR, "Unable to allocate memory for device's PNL.");
        } else {
            snprintf(msg, 48, "Unable to allocate %d bytes for device's PNL.", sizeof(char) * nets_count);
            wendigo_log(MSG_ERROR, msg);
            wendigo_display_popup(app, "Insufficient memory", msg);
            free(msg);
        }
        FURI_LOG_T(WENDIGO_TAG, "End get_networks_for_device() - Unable to initialise results array.");
        return 0;
    }
    for (uint8_t i = 0; i < nets_count; ++i) {
        if (dev->radio.sta.saved_networks[i] == NULL ||
                strlen(dev->radio.sta.saved_networks[i]) == 0) {
            res[i] = malloc(sizeof(char) * (strlen(dev->radio.sta.saved_networks[i]) + 1));
            if (res[i] == NULL) {
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
    FURI_LOG_T(WENDIGO_TAG, "End get_networks_for_device()");
    return nets_count;
}

// TODO: May not actually need this function?
uint16_t get_all_networks(WendigoApp *app) {
    FURI_LOG_T(WENDIGO_TAG, "Start get_all_networks()");
    char **nets;
    int16_t count = 0;
    /* First find the size of our array */
    for (uint16_t i = 0; i < devices_count; ++i) {
        if (devices != NULL && devices[i] != NULL && devices[i]->scanType == SCAN_WIFI_STA) {
            count += count_networks_for_device(devices[i]);
        }
    }
    nets = malloc(sizeof(char *) * count);
    if (nets == NULL) {
        char *msg = malloc(sizeof(char) * 44);
        if (msg == NULL) {
            wendigo_log(MSG_ERROR, "Failed to allocate memory to stringify PNLs.");
        } else {
            snprintf(msg, 44, "Failed to allocatte memory for %d PNLs.", count);
            wendigo_log(MSG_ERROR, msg);
            free(msg);
        }
        FURI_LOG_T(WENDIGO_TAG, "End get_all_networks() - Can't allocate SSID array.");
        return 0;
    }
    /* Call get_networks_for_device() on each device and concatenate their results */
    char **dev_nets;
    uint8_t dev_count;
    for (uint16_t i = 0; i < devices_count; ++i) {
        if (devices != NULL && devices[i] != NULL && devices[i]->scanType == SCAN_WIFI_STA) {
            dev_count = get_networks_for_device(app, devices[i], &dev_nets);
            if (dev_count > 0 && dev_nets != NULL) {
                /* Copy dev_nets into nets */
                // TODO Maintain an index into nets[].
            }
        }
    }
    FURI_LOG_T(WENDIGO_TAG, "End get_all_networks()");
    return 0;
}

/** Scene initialisation. */
void wendigo_scene_pnl_list_on_enter(void *context) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_pnl_list_on_enter()");
    WendigoApp *app = context;
    app->current_view = WendigoAppViewPNLList;
    variable_item_list_reset(app->var_item_list);
    variable_item_list_set_header(app->var_item_list, NULL);
    /* Set enter callback */
    variable_item_list_set_enter_callback(app->var_item_list,
        wendigo_scene_pnl_list_var_list_enter_callback, app);

    wendigo_scene_pnl_list_redraw(app);

    /* Restore selected menu item if it's there */
    uint8_t selected_item = scene_manager_get_scene_state(app->scene_manager, WendigoScenePNLList);
    if (selected_item >= networks_count) {
        selected_item = 0;
    }
    variable_item_list_set_selected_item(app->var_item_list, selected_item);
    view_dispatcher_switch_to_view(app->view_dispatcher, WendigoAppViewVarItemList);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_pnl_list_on_enter()");
}

/** We have no need to respond to events */
bool wendigo_scene_pnl_list_on_event(void *context, SceneManagerEvent event) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_pnl_list_on_event()");
    bool consumed = false;
    if (event.type == SceneManagerEventTypeTick) {
        consumed = true;
    }
    UNUSED(context);
    return consumed;
}

void wendigo_scene_pnl_list_on_exit(void *context) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_pnl_list_on_exit()");
    WendigoApp *app = context;
    variable_item_list_reset(app->var_item_list);
    /* If we hit the back button in this scene, set current_device to NULL */
    wendigo_scene_pnl_list_set_device(NULL, app);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_pnl_list_on_exit()");
}

void wendigo_scene_pnl_list_free() {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_pnl_list_free()");
    if (networks_count > 0 && networks != NULL) {
        for (uint8_t i = 0; i < networks_count; ++i) {
            if (networks[i].device_count > 0 && networks[i].devices != NULL) {
                /* Free networks[i].devices - a wendigo_device** - but don't
                 * free the individual wendigo_device elements. networks[i].devices
                 * should contain pointers to the master device cache devices[].
                 */
                free(networks[i].devices);
                networks[i].devices = NULL;
                networks[i].device_count = 0;
            }
        }
        free(networks);
    }
    networks_count = 0;
    networks = NULL;
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_pnl_list_free()");
}