#include "wendigo_app_i.h"
#include "wendigo_scan.h"
#include "wendigo_pnl.h"

/* PNL cache */
PreferredNetwork *networks = NULL;
uint16_t networks_count = 0;
uint16_t networks_capacity = 0;

/** Search networks[] for a PreferredNetwork with the specified SSID.
 * Returns the index of the PreferredNetwork, or networks_count if not
 * found.
 * SSID must be a null-terminated string.
 */
uint16_t index_of_pnl(char *ssid) {
    FURI_LOG_T(WENDIGO_TAG, "Start index_of_pnl()");
    if (ssid == NULL || strlen(ssid) == 0 || networks == NULL || networks_count == 0) {
        FURI_LOG_T(WENDIGO_TAG, "End index_of_pnl() - Invalid arguments.");
        return 0;
    }
    uint16_t idx;
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
    uint16_t idx = index_of_pnl(ssid);
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
 * Returns the number of associated networks.
 */
uint16_t map_ssids_to_devices(WendigoApp *app) {
    FURI_LOG_T(WENDIGO_TAG, "Start map_ssids_to_devices()");
    if (networks_count > 0 && networks != NULL) {
        /* If networks[] has been initialised we can assume it's up to date */
        return networks_count;
    }

    uint8_t this_count;
    PreferredNetwork *new_networks;
    furi_mutex_acquire(app->pnlMutex, FuriWaitForever);
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
                furi_mutex_release(app->pnlMutex);
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
    furi_mutex_release(app->pnlMutex);
    FURI_LOG_T(WENDIGO_TAG, "End map_ssids_to_devices()");
    return networks_count;
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

PreferredNetwork *fetch_or_create_pnl(char *ssid, uint8_t ssid_len) {
    PreferredNetwork *pnl;
    uint16_t idx = index_of_pnl(ssid);
    if (idx == networks_count) {
        /* Not found - Create a new PreferredNetwork */
        if (networks_count == networks_capacity) {
            /* No spare capacity - Extend networks[] */
            pnl = realloc(networks, sizeof(PreferredNetwork) * (networks_count + 1));
            if (pnl == NULL) {
                char *msg = malloc(sizeof(char) * (82 + MAX_SSID_LEN));
                if (msg == NULL) {
                    wendigo_log(MSG_ERROR,
                        "wendigo_add_device(): Failed to increase networks[], skipping PNL.");
                } else {
                    snprintf(msg, 82 + MAX_SSID_LEN,
                        "wendigo_add_device(): Failed to increase networks[] to %d bytes, skipping PNL %s.",
                        sizeof(PreferredNetwork) * (networks_count + 1), ssid);
                    wendigo_log(MSG_ERROR, msg);
                    free(msg);
                }
            } else {
                /* Allocated successfully */
                networks = pnl;
                ++networks_capacity;
            }
        }
        if (networks_count < networks_capacity) {
            /* Allocated successfully or had spare capacity - Initialise */
            bzero(networks + (sizeof(PreferredNetwork) * networks_count), sizeof(PreferredNetwork));
            idx = networks_count++;
            strncpy(networks[idx].ssid, ssid, ssid_len);
        }
    }
    if (idx < networks_count) {
        return &(networks[idx]);
    }
    return NULL;
}

uint8_t pnl_index_of_mac(PreferredNetwork *pnl, uint8_t mac[MAC_BYTES]) {
    UNUSED(pnl);
    UNUSED(mac);
    return 0;
}

uint8_t pnl_index_of_device(PreferredNetwork *pnl, wendigo_device *dev) {
    return pnl_index_of_mac(pnl, dev->mac);
}