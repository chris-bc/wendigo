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

void wendigo_scene_pnl_list_set_device(wendigo_device *dev) {
    current_device = dev;
    // TODO: if (app->current_view == WendigoAppViewPNLList) redraw() - Need to get context (WendigoApp *)
}

wendigo_device *wendigo_scene_pnl_list_get_device() {
    return current_device;
}

/** Search networks[] for a PreferredNetwork with the specified SSID.
 * Returns the index of the PreferredNetwork, or networks_count if not
 * found.
 * SSID must be a null-terminated string.
 */
uint8_t index_of_pnl(char *ssid) {
    if (ssid == NULL || strlen(ssid) == 0 || networks == NULL || networks_count == 0) {
        return 0;
    }
    uint8_t idx;
    uint8_t compareLen = MAX_SSID_LEN;
    if (strlen(ssid) < compareLen) {
        compareLen = strlen(ssid);
    }
    for (idx = 0; idx < networks_count && strncmp(ssid, networks[idx].ssid, compareLen); ++idx) { }
    return idx;
}

/** Find the PreferredNetwork element of networks[] containing the
 * specified SSID. Returns a reference to the element of networks[]
 * or NULL if not found;
 * SSID must be a null-terminated string.
 */
PreferredNetwork *pnl_for_ssid(char *ssid) {
    PreferredNetwork *result = NULL;
    uint8_t idx = index_of_pnl(ssid);
    if (idx < networks_count) {
        result = &(networks[idx]);
    }
    return result;
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

/** Process the device cache (devices[]) and generate PreferredNetwork instances
 * that map all identified networks to the wendigo_device instances that have
 * probed for them.
 * networks[] and networks_count are updated by this function.
 */
uint8_t map_ssids_to_devices(WendigoApp *app) {
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
                return 0;
            }
            networks_capacity = networks_count + this_count;
            networks = new_networks;
        }
        /* Check each saved network for devices[i] */
        for (uint8_t j = 0; j < this_count; ++j) {
            // if SSID devices[i]->radio.sta.saved_networks[j] not in networks[]
            //      Append a new PreferredNetwork
            // Append devices[i] to SSID's PreferredNetwork
        }
    }
    if (networks_capacity > networks_count) {
        new_networks = realloc(networks, sizeof(PreferredNetwork) * networks_count);
        if (new_networks == NULL) {
            /* This condition will (should) never be true */
            wendigo_log(MSG_ERROR, "Unable to shrink networks[] to remove spare capacity.");
            /* Don't fret too much - although should probably bzero() new capacity so NULL checks will work */
            // TODO
        } else {
            networks = new_networks;
        }
    }
    return networks_count;
}

// TODO: May not actually need this function?
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
