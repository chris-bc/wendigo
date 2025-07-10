#include "wendigo_scan.h"
#include "wendigo_app_i.h"
#include "wendigo_common_defs.h"

uint8_t *buffer = NULL;
uint16_t bufferLen = 0; // 65535 should be plenty of length
uint16_t bufferCap = 0; // Buffer capacity - I don't want to allocate 65kb, but
                        // don't want to constantly realloc
char *wendigo_popup_text = NULL; // I suspect the popup text is going out of
                                 // scope when declared at function scope

/* Internal function declarations */
uint16_t custom_device_index(wendigo_device *dev, wendigo_device **array,
                            uint16_t array_count);

/* Device caches */
wendigo_device **devices = NULL;
wendigo_device **selected_devices = NULL;
uint16_t devices_count = 0;
uint16_t selected_devices_count = 0;
uint16_t devices_capacity = 0;
uint16_t selected_devices_capacity = 0;

/* How much will we increase bt_devices[] by when additional space is needed? */
#define INC_DEVICE_CAPACITY_BY 10
/* Maximum size of UART buffer - If a packet terminator isn't found within this
   region older data will be removed */
#define BUFFER_MAX_SIZE 4096
/* How much will we increase buffer[] by when additional space is needed? */
#define BUFFER_INC_CAPACITY_BY 128

/** Search for a start-of-packet marker in the specified byte array
 *  This function is used during parsing to skip past any extraneous bytes
 *  that are sent prior to the start-of-packet sequence.
 *  Returns `size` if not found, otherwise returns the index of the first byte
 * of the sequence.
 */
uint16_t start_of_packet(uint8_t *bytes, uint16_t size) {
    FURI_LOG_T(WENDIGO_TAG, "Start start_of_packet()");
    uint16_t result = 0;
    if (bytes == NULL) {
        return size;
    }
    for (; result + PREAMBLE_LEN <= size &&
            memcmp(bytes + result, PREAMBLE_BT_BLE, PREAMBLE_LEN) &&
            memcmp(bytes + result, PREAMBLE_WIFI_AP, PREAMBLE_LEN) &&
            memcmp(bytes + result, PREAMBLE_WIFI_STA, PREAMBLE_LEN) &&
            memcmp(bytes + result, PREAMBLE_STATUS, PREAMBLE_LEN) &&
            memcmp(bytes + result, PREAMBLE_CHANNELS, PREAMBLE_LEN) &&
            memcmp(bytes + result, PREAMBLE_VER, PREAMBLE_LEN);
        ++result) {
    }
    /* If not found, set result to size */
    if (result + PREAMBLE_LEN > size) {
        result = size;
    }
    FURI_LOG_T(WENDIGO_TAG, "End start_of_packet()");
    return result;
}

/** Search for the end-of-packet marker in the specified byte array
 *  A packet is terminated by a sequence of 4*0xAA and 4*0xFF.
 *  Returns the index of the last byte in this sequence, or the
 *  specified `size` if not found.
 */
uint16_t end_of_packet(uint8_t *theBytes, uint16_t size) {
    FURI_LOG_T(WENDIGO_TAG, "Start end_of_packet()");
    uint16_t result = PREAMBLE_LEN - 1; /* Start by checking bytes [0] through [7] */
    if (theBytes == NULL) {
        return size;
    }
    for (; result < size && memcmp(theBytes + result - PREAMBLE_LEN + 1,
                                    PACKET_TERM, PREAMBLE_LEN);
        ++result) {
    }
    FURI_LOG_T(WENDIGO_TAG, "End end_of_packet()");
    return result;
}

/** Returns the index of the first occurrence of `\n` in `theString`, searching
 *  the first `size` characters. Returns size if not found (which is outside the
 *  array bounds).
 *  TODO: This function is a candidate for deletion now that packets use an
 *  alternate packet terminator
 */
uint16_t bytes_contains_newline(uint8_t *theBytes, uint16_t size) {
    FURI_LOG_T(WENDIGO_TAG, "Start bytes_contains_newline()");
    if (theBytes == NULL) {
        return size;
    }
    uint16_t result = 0;
    for (; result < size && theBytes[result] != '\n'; ++result) { }
    FURI_LOG_T(WENDIGO_TAG, "End bytes_contains_newline()");
    return result;
}

/** Send the status command to ESP32 */
void wendigo_esp_status(WendigoApp *app) {
    char cmd[] = "s\n";
    wendigo_uart_tx(app->uart, (uint8_t *)cmd, strlen(cmd) + 1);
}

/** Send the version command to ESP32 */
void wendigo_version(WendigoApp *app) {
    char cmd[] = "v\n";
    wendigo_uart_tx(app->uart, (uint8_t *)cmd, strlen(cmd) + 1);
}

/** Manage the tagged/selected status of the specified wendigo_device
 *  Maintains selected_devices[] as needed
 */
bool wendigo_set_device_selected(wendigo_device *device, bool selected) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_set_device_selected()");
    /* NULL device check */
    if (device == NULL) {
        return false;
    }
    if (selected && !device->tagged) {
        /* Add device to selected_devices[] */
        if (selected_devices_capacity == selected_devices_count) {
            /* Extend selected_devices[] */
            wendigo_device **new_devices = realloc(selected_devices,
                sizeof(wendigo_device *) * (selected_devices_capacity + 1));
            if (new_devices == NULL) {
                char msg[65];
                snprintf(msg, sizeof(msg), "Failed to expand selected_devices[] to hold %d wendigo_device*.",
                    selected_devices_capacity + 1);
                wendigo_log(MSG_ERROR, msg);
                return false;
            }
            selected_devices = new_devices;
            ++selected_devices_capacity;
        }
        selected_devices[selected_devices_count++] = device;
        // TODO: Sort
    } else if (device->tagged && !selected) {
        /* Remove device from selected_devices[] - custom_device_index() will safely
         * handle any NULLs */
        /* Not using 16 bits because nobody's going to select 255 devices */
        uint8_t index = (uint8_t)custom_device_index(device, selected_devices,
                                                        selected_devices_count);
        if (index < selected_devices_count) {
            /* Shuffle forward all subsequent devices, replacing device at `index` */
            for (++index; index < selected_devices_count; ++index) {
                selected_devices[index - 1] = selected_devices[index];
            }
            selected_devices[selected_devices_count--] = NULL;
        }
    }
    device->tagged = selected;
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_set_device_selected()");
    return true;
}

/** Enable or disable Wendigo scanning on all interfaces, using app->interfaces
 *  to determine which radios should be enabled/disabled when starting to scan.
 *  This function is called by the UI handler (wendigo_scene_start) when scanning
 *  is started or stopped. app->interfaces is updated via the Setup menu.
 *  Because wendigo_uart_tx returns void there is no way to identify failure,
 *  so this function also returns void.
 */
void wendigo_set_scanning_active(WendigoApp *app, bool starting) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_set_scanning_active()");
    char cmd;
    uint8_t arg;
    const uint8_t CMD_LEN = 5; // e.g. "b 1\n\0"
    char cmdString[CMD_LEN];
    /* This flag will cause incomplete packets to be ignored */
    app->is_scanning = starting;
    for (uint8_t i = 0; i < IF_COUNT; ++i) {
        /* Set command */
        cmd = (i == IF_BLE) ? 'b' : (i == IF_BT_CLASSIC) ? 'h' : 'w';
        /* arg */
        arg = (starting && app->interfaces[i].active) ? 1 : 0;
        snprintf(cmdString, CMD_LEN, "%c %d\n", cmd, arg);
        wendigo_uart_tx(app->uart, (uint8_t *)cmdString, CMD_LEN);
    }
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_set_scanning_active()");
}

/** Returns the index into `array` of the device with MAC/BDA matching dev->mac.
 *  Returns `array_count` if the device was not found.
 */
uint16_t custom_device_index(wendigo_device *dev, wendigo_device **array,
                            uint16_t array_count) {
    FURI_LOG_T(WENDIGO_TAG, "Start custom_device_index()");
    uint16_t result;
    if (dev == NULL || array == NULL) {
        FURI_LOG_T(WENDIGO_TAG, "End custom_device_index() ABNORMAL");
        return array_count;
    }
    for (result = 0; result < array_count && (array[result] == NULL ||
                    memcmp(dev->mac, array[result]->mac, MAC_BYTES));
                    ++result) { }
    FURI_LOG_T(WENDIGO_TAG, "End custom_device_index()");
    return result;
}

/** Returns the index into devices[] of the device with MAC/BDA matching
 *  dev->mac. Returns devices_count if the device was not found. A NULL value for
 *  `dev` is handled correctly by custom_device_index()
 */
uint16_t device_index(wendigo_device *dev) {
    FURI_LOG_T(WENDIGO_TAG, "Start+End device_index()");
    return custom_device_index(dev, devices, devices_count);
}

/** Returns the index into devices[] of the device with MAC/BDA matching mac.
 *  Returns devices_count if the device was not found.
 */
uint16_t device_index_from_mac(uint8_t mac[MAC_BYTES]) {
    FURI_LOG_T(WENDIGO_TAG, "Start device_index_from_mac()");
    wendigo_device dev;
    memcpy(dev.mac, mac, MAC_BYTES);
    FURI_LOG_T(WENDIGO_TAG, "End device_index_from_mac()");
    return device_index(&dev);
}

/** Determines whether a device with the MAC/BDA dev->mac exists in devices[].
 *  A NULL value for `dev` is handled correctly at the top of the call stack,
 *  by custom_device_index().
 */
bool device_exists(wendigo_device *dev) {
    return device_index(dev) < devices_count;
}

/** Check whether the provided MAC is present in the specified array of MACs.
 * Returns the index of the matching array element, or array_len if not found.
 */
uint16_t wendigo_index_of(uint8_t mac[MAC_BYTES], uint8_t **array, uint16_t array_len) {
    if (mac == NULL || !memcmp(mac, nullMac, MAC_BYTES) || array == NULL || array_len == 0) {
        return array_len;
    }
    uint16_t idx = 0;
    for (; idx < array_len && (array[idx] == NULL || memcmp(array[idx], mac, MAC_BYTES)); ++idx) { }
    return idx;
} // TODO: this and device_index_* functions are also defined in ESP32-Wendigo, in common.c. Merge and move to wendigo_common_defs.c

/** Called from parseBufferBluetooth(), this function updates the existing device
 *  `dev` with new attributes from `new_device`.
 *  Returns a boolean representing the success or failure of the function.
 */
bool wendigo_add_bt_device(wendigo_device *dev, wendigo_device *new_device) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_add_bt_device()");
    new_device->radio.bluetooth.cod = dev->radio.bluetooth.cod;
    if (dev->radio.bluetooth.cod_str != NULL) {
        new_device->radio.bluetooth.cod_str =
            malloc(sizeof(char) * (strlen(dev->radio.bluetooth.cod_str) + 1));
        if (new_device->radio.bluetooth.cod_str != NULL) {
            /* No need to handle allocation failure - we simply don't copy cod_str */
            strncpy(new_device->radio.bluetooth.cod_str, dev->radio.bluetooth.cod_str,
                strlen(dev->radio.bluetooth.cod_str));
            new_device->radio.bluetooth.cod_str[strlen(dev->radio.bluetooth.cod_str)] = '\0';
        }
    } else {
        new_device->radio.bluetooth.cod_str = NULL;
    }
    /* Device name */
    new_device->radio.bluetooth.bdname_len = dev->radio.bluetooth.bdname_len;
    if (dev->radio.bluetooth.bdname_len > 0 && dev->radio.bluetooth.bdname != NULL) {
        new_device->radio.bluetooth.bdname = malloc(sizeof(char) * (dev->radio.bluetooth.bdname_len + 1));
        if (new_device->radio.bluetooth.bdname == NULL) {
            /* Skip the device's bdname and hope we have memory for it next time it's seen */
            new_device->radio.bluetooth.bdname_len = 0;
        } else {
            strncpy(new_device->radio.bluetooth.bdname, dev->radio.bluetooth.bdname,
                    dev->radio.bluetooth.bdname_len);
            new_device->radio.bluetooth.bdname[dev->radio.bluetooth.bdname_len] = '\0';
        }
    } else {
        new_device->radio.bluetooth.bdname = NULL;
        new_device->radio.bluetooth.bdname_len = 0;
    }
    /* EIR */
    new_device->radio.bluetooth.eir_len = dev->radio.bluetooth.eir_len;
    if (dev->radio.bluetooth.eir_len > 0 && dev->radio.bluetooth.eir != NULL) {
        new_device->radio.bluetooth.eir = malloc(sizeof(uint8_t) * dev->radio.bluetooth.eir_len);
        if (new_device->radio.bluetooth.eir == NULL) {
            /* Skip EIR from this packet */
            new_device->radio.bluetooth.eir_len = 0;
        } else {
            memcpy(new_device->radio.bluetooth.eir, dev->radio.bluetooth.eir,
                dev->radio.bluetooth.eir_len);
        }
    } else {
        new_device->radio.bluetooth.eir = NULL;
        new_device->radio.bluetooth.eir_len = 0;
    }
    /* BT services */
    new_device->radio.bluetooth.bt_services.num_services = dev->radio.bluetooth.bt_services.num_services;
    if (dev->radio.bluetooth.bt_services.num_services > 0 &&
            dev->radio.bluetooth.bt_services.service_uuids != NULL) {
        new_device->radio.bluetooth.bt_services.service_uuids =
            malloc(sizeof(void *) * dev->radio.bluetooth.bt_services.num_services);
        if (new_device->radio.bluetooth.bt_services.service_uuids == NULL) {
            new_device->radio.bluetooth.bt_services.num_services = 0;
        } else {
            memcpy(new_device->radio.bluetooth.bt_services.service_uuids,
                dev->radio.bluetooth.bt_services.service_uuids,
                sizeof(void *) * dev->radio.bluetooth.bt_services.num_services);
        }
    } else {
        new_device->radio.bluetooth.bt_services.service_uuids = NULL;
        new_device->radio.bluetooth.bt_services.num_services = 0;
    }
    /* Known services */
    new_device->radio.bluetooth.bt_services.known_services_len =
            dev->radio.bluetooth.bt_services.known_services_len;
    if (dev->radio.bluetooth.bt_services.known_services_len > 0 &&
            dev->radio.bluetooth.bt_services.known_services != NULL) {
        /* known_services is an array of pointers - this is where we stop
            controlling memory allocation so that bt_uuid's can be allocated a single
            time and reused. This function will copy the *pointers* in
            known_services[], but not the bt_uuid structs that they point to. */
        new_device->radio.bluetooth.bt_services.known_services = malloc(sizeof(bt_uuid *) *
                dev->radio.bluetooth.bt_services.known_services_len);
        if (new_device->radio.bluetooth.bt_services.known_services == NULL) {
            new_device->radio.bluetooth.bt_services.known_services_len = 0;
        } else {
            memcpy(new_device->radio.bluetooth.bt_services.known_services,
                dev->radio.bluetooth.bt_services.known_services,
                sizeof(bt_uuid *) * dev->radio.bluetooth.bt_services.known_services_len);
            // TODO: Despite the caveat above, it might be nice to validate that the
            // bt_uuid's we're pointing to actually exist.
        }
    } else {
        new_device->radio.bluetooth.bt_services.known_services = NULL;
        new_device->radio.bluetooth.bt_services.known_services_len = 0;
    }
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_add_bt_device()");
    return true;
}

/** Updates the existing device `dev` based on the newly-received device `new_device`.
 *  This function is called from parseBufferBluetooth() when a bluetooth packet
 *  referring to an existing device is received.
 *  Returns a boolean indicating the success/failure of the function.
 */
bool wendigo_update_bt_device(wendigo_device *dev, wendigo_device *new_device) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_update_bt_device()");
    new_device->radio.bluetooth.cod = dev->radio.bluetooth.cod;
    /* cod_str present in update? */
    if (dev->radio.bluetooth.cod_str != NULL && strlen(dev->radio.bluetooth.cod_str) > 0) {
        char *new_cod = realloc(new_device->radio.bluetooth.cod_str,
                sizeof(char) * (strlen(dev->radio.bluetooth.cod_str) + 1));
        if (new_cod != NULL) {
            strncpy(new_cod, dev->radio.bluetooth.cod_str, strlen(dev->radio.bluetooth.cod_str));
            new_cod[strlen(dev->radio.bluetooth.cod_str)] = '\0';
            new_device->radio.bluetooth.cod_str = new_cod;
        }
    }
    /* Is bdname in update? */
    if (dev->radio.bluetooth.bdname_len > 0 && dev->radio.bluetooth.bdname != NULL) {
        char *new_name = realloc(new_device->radio.bluetooth.bdname,
                sizeof(char) * (dev->radio.bluetooth.bdname_len + 1));
        if (new_name != NULL) { /* If allocation fails target's existing bdname and
                                    bdname_len are unmodified */
            strncpy(new_name, dev->radio.bluetooth.bdname, dev->radio.bluetooth.bdname_len);
            new_name[dev->radio.bluetooth.bdname_len] = '\0';
            new_device->radio.bluetooth.bdname = new_name;
            new_device->radio.bluetooth.bdname_len = dev->radio.bluetooth.bdname_len;
        }
    }
    /* How about EIR? */
    if (dev->radio.bluetooth.eir_len > 0 && dev->radio.bluetooth.eir != NULL) {
        uint8_t *new_eir = realloc(new_device->radio.bluetooth.eir,
                                   sizeof(uint8_t) * dev->radio.bluetooth.eir_len);
        if (new_eir != NULL) {
            memcpy(new_eir, dev->radio.bluetooth.eir, sizeof(uint8_t) * dev->radio.bluetooth.eir_len);
            new_device->radio.bluetooth.eir = new_eir;
            new_device->radio.bluetooth.eir_len = dev->radio.bluetooth.eir_len;
        }
    }
    /* Number of services */
    if (dev->radio.bluetooth.bt_services.num_services > 0 &&
            dev->radio.bluetooth.bt_services.service_uuids != NULL) {
        void *new_svcs = realloc(new_device->radio.bluetooth.bt_services.service_uuids,
                sizeof(void *) * dev->radio.bluetooth.bt_services.num_services);
        if (new_svcs != NULL) {
            memcpy(new_svcs, dev->radio.bluetooth.bt_services.service_uuids,
                sizeof(void *) * dev->radio.bluetooth.bt_services.num_services);
            new_device->radio.bluetooth.bt_services.service_uuids = new_svcs;
            new_device->radio.bluetooth.bt_services.num_services =
                dev->radio.bluetooth.bt_services.num_services;
        }
    }
    /* Known services */
    if (dev->radio.bluetooth.bt_services.known_services_len > 0 &&
            dev->radio.bluetooth.bt_services.known_services != NULL) {
        bt_uuid **new_known_svcs = realloc(new_device->radio.bluetooth.bt_services.known_services,
                sizeof(bt_uuid *) * dev->radio.bluetooth.bt_services.known_services_len);
        if (new_known_svcs != NULL) {
            memcpy(new_known_svcs, dev->radio.bluetooth.bt_services.known_services, sizeof(bt_uuid *) *
                    dev->radio.bluetooth.bt_services.known_services_len);
            new_device->radio.bluetooth.bt_services.known_services = new_known_svcs;
            new_device->radio.bluetooth.bt_services.known_services_len =
                    dev->radio.bluetooth.bt_services.known_services_len;
        }
    }
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_update_bt_device()");
    return true;
}

/** Add the specified device to devices[], extending the length of devices[] if
 * necessary. If the specified device has a MAC/BDA which is already present in
 * devices[] a new entry will not be made, instead the element with the same
 * MAC/BDA will be updated based on the specified device. Returns true if the
 * device was successfully added to (or updated in) devices[]. NOTE: The calling
 * function may free the specified wendigo_device or any of its attributes when
 * this function returns. To minimise the likelihood of memory leaks this
 * function will allocate its own memory to hold the specified device and its
 * attributes.
 */
bool wendigo_add_device(WendigoApp *app, wendigo_device *dev) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_add_device()");
    uint16_t idx = device_index(dev);
    if (idx < devices_count) {
        /* A device with the provided BDA already exists - Update that instead */
        return wendigo_update_device(app, dev);
    }
    /* Adding to devices - Increase capacity by an additional INC_DEVICE_CAPACITY_BY if necessary */
    if (devices == NULL || devices_capacity == devices_count) {
        wendigo_device **new_devices = realloc(devices, sizeof(wendigo_device *) *
                                        (devices_capacity + INC_DEVICE_CAPACITY_BY));
        if (new_devices == NULL) {
            /* Can't store the device */
            return false;
        }
        devices = new_devices;
        devices_capacity += INC_DEVICE_CAPACITY_BY;
    }
    devices[devices_count] = malloc(sizeof(wendigo_device));
    if (devices[devices_count] == NULL) {
        /* That's unfortunate */
        return false;
    }
    wendigo_device *new_device = devices[devices_count++];
    /* Copy common attributes */
    new_device->rssi = dev->rssi;
    new_device->scanType = dev->scanType;
    /* Don't copy tagged status - A new device is not tagged. */
    new_device->tagged = false;
    new_device->view = dev->view; /* Copy the view pointer if we have one, NULL if we don't */
    /* ESP32 doesn't know the real time/date so overwrite the lastSeen value.
       time_t is just another way of saying long long int, so casting is OK */
    new_device->lastSeen = furi_hal_rtc_get_timestamp();
    /* Copy MAC/BDA */
    memcpy(new_device->mac, dev->mac, MAC_BYTES);
    /* Copy protocol-specific attributes */
    if (dev->scanType == SCAN_HCI || dev->scanType == SCAN_BLE) {
        wendigo_add_bt_device(dev, new_device);
    } else if (dev->scanType == SCAN_WIFI_AP) {
        new_device->radio.ap.channel = dev->radio.ap.channel;
        new_device->radio.ap.stations_count = dev->radio.ap.stations_count;
        if (dev->radio.ap.stations_count > 0) {
            /* Copy stations[] */
            new_device->radio.ap.stations = malloc(sizeof(uint8_t *) * dev->radio.ap.stations_count);
            if (new_device->radio.ap.stations == NULL) {
                new_device->radio.ap.stations_count = 0;
            } else {
                for (uint8_t i = 0; i < dev->radio.ap.stations_count; ++i) {
                    new_device->radio.ap.stations[i] = malloc(MAC_BYTES);
                    if (new_device->radio.ap.stations[i] != NULL) {
                        memcpy(new_device->radio.ap.stations[i], dev->radio.ap.stations[i], MAC_BYTES);
                    }
                }
            }
        } else {
            new_device->radio.ap.stations = NULL;
        }
        memcpy(new_device->radio.ap.ssid, dev->radio.ap.ssid, MAX_SSID_LEN);
        new_device->radio.ap.ssid[MAX_SSID_LEN] = '\0';
    } else if (dev->scanType == SCAN_WIFI_STA) {
        new_device->radio.sta.channel = dev->radio.sta.channel;
        memcpy(new_device->radio.sta.apMac, dev->radio.sta.apMac, MAC_BYTES);
    }

    /* If the device list scene is currently displayed, add the device to the UI */
    if (app->current_view == WendigoAppViewDeviceList) {
        wendigo_scene_device_list_update(app, new_device);
    }
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_add_device()");
    return true;
}

/** Locate the device in devices[] with the same MAC/BDA as `dev` and update the
 * object based on the contents of `dev`. If a device with the specified MAC/BDA
 * does not exist a new device will be added to devices[]. Returns true if the
 * specified device has been updated (or a new device added) successfully. NOTE:
 * The calling function may free `dev` or any of its members immediately upon
 * this function returning. To minimise the likelihood of memory leaks, the
 * device cache management functions manage the allocation and free'ing of all
 * components of the device cache *except for* bt_uuid objects that form part of
 * service descriptors.
 */
bool wendigo_update_device(WendigoApp *app, wendigo_device *dev) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_update_device()");
    uint16_t idx = device_index(dev);
    if (idx == devices_count) {
        /* Device doesn't exist in bt_devices[] - Add it instead */
        return wendigo_add_device(app, dev);
    }
    wendigo_device *target = devices[idx];
    /* Copy common attributes */
    target->rssi = dev->rssi;
    target->scanType = dev->scanType;
    /* Replace lastSeen */
    target->lastSeen = furi_hal_rtc_get_timestamp();
    /* Copy protocol-specific attributes */
    if (dev->scanType == SCAN_HCI || dev->scanType == SCAN_BLE) {
        wendigo_update_bt_device(dev, target);
    } else if (dev->scanType == SCAN_WIFI_AP) {
        /* Copy channel, ssid, sta_count, stations */
        target->radio.ap.channel = dev->radio.ap.channel;
        uint8_t ssid_len = strnlen((char *)dev->radio.ap.ssid, MAX_SSID_LEN);
        if (ssid_len > 0) {
            memcpy(target->radio.ap.ssid, dev->radio.ap.ssid, ssid_len + 1);
            target->radio.ap.ssid[ssid_len] = '\0';
        }
        /* Merge dev->radio.ap.stations with target->radio.ap.stations */
        uint8_t new_stations = 0;
        if (target->radio.ap.stations_count == 0) {
            new_stations = dev->radio.ap.stations_count;
        } else {
            /* Loop through dev->radio.ap.stations and count the MACs not present in target->radio.ap.stations */
            for (uint8_t i = 0; i < dev->radio.ap.stations_count; ++i) {
                if (wendigo_index_of(dev->radio.ap.stations[i], target->radio.ap.stations,
                        target->radio.ap.stations_count) == target->radio.ap.stations_count) {
                    ++new_stations;
                }
            }
        }
        if (new_stations > 0) {
            /* Append new_stations elements to target->radio.ap.stations */
            uint8_t **updated_stations = realloc(target->radio.ap.stations,
                sizeof(uint8_t *) * (target->radio.ap.stations_count + new_stations));
            if (updated_stations != NULL) {
                uint8_t stationIdx = target->radio.ap.stations_count;
                for (uint8_t i = 0; i < dev->radio.ap.stations_count; ++i) {
                    if (wendigo_index_of(dev->radio.ap.stations[i], updated_stations,
                            target->radio.ap.stations_count) == target->radio.ap.stations_count) {
                        /* Add dev->radio.ap.stations[i] */
                        updated_stations[stationIdx] = malloc(MAC_BYTES);
                        if (updated_stations[stationIdx] != NULL) {
                            memcpy(updated_stations[stationIdx], dev->radio.ap.stations[i], MAC_BYTES);
                        }
                        ++stationIdx;
                    }
                }
                target->radio.ap.stations = updated_stations;
                target->radio.ap.stations_count += new_stations;
            }
        }
    } else if (dev->scanType == SCAN_WIFI_STA) {
        /* Copy channel, AP MAC, AP */
        target->radio.sta.channel = dev->radio.sta.channel;
        /* Only replace AP MAC if dev has a "better" AP than target - If dev isn't nullMac. */
        if (memcmp(dev->radio.sta.apMac, nullMac, MAC_BYTES)) {
            memcpy(target->radio.sta.apMac, dev->radio.sta.apMac, MAC_BYTES);
        }
    }
    /* Update the device list if it's currently displayed */
    if (app->current_view == WendigoAppViewDeviceList) {
        wendigo_scene_device_list_update(app, target);
    } else if (app->current_view == WendigoAppViewDeviceDetail) { // && selectedDevice == bt_devices[idx]
        // TODO: Update existing view
    }
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_update_device()");
    return true;
}

/** Free all memory allocated to the specified device.
 * After free'ing its members this function will also free `dev` itself.
 * dev->view will not be touched by this function - It is allocated and freed in
 * wendigo_scene_device_list.c. NOTE: I've gone back and forth between setting
 * attributes to NULL and 0 after freeing and not (because the attributes aren't
 * accessible after freeing `dev`), but have finally made a decision: Attributes
 * representing allocated memory ARE set to NULL (for pointers) and 0 (for
 * counts) to reduce the likelihood that any orphan pointers will attempt to use
 * deallocated memory.
 */
void wendigo_free_device(wendigo_device *dev) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_free_device()");
    if (dev == NULL) {
        return;
    }
    if (dev->scanType == SCAN_HCI || dev->scanType == SCAN_BLE) {
        if (dev->radio.bluetooth.cod_str != NULL) {
            free(dev->radio.bluetooth.cod_str);
            dev->radio.bluetooth.cod_str = NULL;
        }
        if (dev->radio.bluetooth.bdname != NULL) {
            free(dev->radio.bluetooth.bdname);
            dev->radio.bluetooth.bdname = NULL;
            dev->radio.bluetooth.bdname_len = 0;
        }
        if (dev->radio.bluetooth.eir != NULL) {
            free(dev->radio.bluetooth.eir);
            dev->radio.bluetooth.eir = NULL;
        }
        if (dev->radio.bluetooth.bt_services.service_uuids != NULL) {
            free(dev->radio.bluetooth.bt_services.service_uuids);
            dev->radio.bluetooth.bt_services.service_uuids = NULL;
            dev->radio.bluetooth.bt_services.num_services = 0;
        }
        if (dev->radio.bluetooth.bt_services.known_services != NULL) {
            // TODO: After implementing BT services, ensure that the bt_uuid's are
            // freed somewhere
            free(dev->radio.bluetooth.bt_services.known_services);
            dev->radio.bluetooth.bt_services.known_services = NULL;
            dev->radio.bluetooth.bt_services.known_services_len = 0;
        }
    } else if (dev->scanType == SCAN_WIFI_AP) {
        if (dev->radio.ap.stations != NULL && dev->radio.ap.stations_count > 0) {
            for (uint8_t i = 0; i < dev->radio.ap.stations_count; ++i) {
                if (dev->radio.ap.stations[i] != NULL) {
                    free(dev->radio.ap.stations[i]);
                }
            }
            free(dev->radio.ap.stations);
            dev->radio.ap.stations = NULL;
            dev->radio.ap.stations_count = 0;
        }
    } else if (dev->scanType == SCAN_WIFI_STA) {
        /* Nothing to do */
    } else {
        /* Nothing to do */
    }
    dev->view = NULL;
    free(dev);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_free_device()");
}

/** Deallocates all memory allocated to the device cache.
 * This function deallocates all elements of the device cache, devices[] and
 * selected_devices[]. These arrays are left in a coherent state, with the
 * arrays set to NULL and their count & capacity variables set to zero.
 */
void wendigo_free_devices() {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_free_devices()");
    /* Start by freeing selected_devices[] */
    if (selected_devices_capacity > 0 && selected_devices != NULL) {
        free(selected_devices); /* Elements will be freed when addressing
                                   bt_devices[] */
        selected_devices = NULL;
        selected_devices_count = 0;
        selected_devices_capacity = 0;
    }
    /* For devices[] we also want to free device attributes and the devices
     * themselves */
    if (devices_capacity > 0 && devices != NULL) {
        for (uint16_t i = 0; i < devices_count; ++i) {
            wendigo_free_device(devices[i]);
            devices[i] = NULL;
        }
        free(devices);
        devices = NULL;
        devices_count = 0;
        devices_capacity = 0;
    }
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_free_devices()");
}

/** During testing I bumped the ESP32 reset button and was irritated that
 * Flipper's device cache was working perfectly but status claimed there were no
 * devices in the cache despite displaying them). This function provides a
 * location to validate and alter status attributes prior to displaying them,
 * allowing this issue to be overcome.
 */
void process_and_display_status_attribute(WendigoApp *app, char *attribute_name,
                                          char *attribute_value) {
    FURI_LOG_T(WENDIGO_TAG, "Start process_and_display_status_attribute()");
    char *interesting_attributes[] = {"BT Classic Devices:", "BT Low Energy Devices:",
        "WiFi STA Devices:", "WiFi APs:"};
    uint8_t interesting_attributes_count = 4;
    uint8_t interested;
    uint16_t count = 0;
    char strVal[6];
    for (interested = 0; interested < interesting_attributes_count &&
            strcmp(interesting_attributes[interested], attribute_name);
        ++interested) { }
    if (interested < interesting_attributes_count) {
        /* We're interested in this attribute */
        for (uint16_t i = 0; i < devices_count; ++i) {
            /* Does the current device contribute to the interesting attribute count? */
            if ((interested == 0 && devices[i]->scanType == SCAN_HCI) ||
                (interested == 1 && devices[i]->scanType == SCAN_BLE) ||
                (interested == 2 && devices[i]->scanType == SCAN_WIFI_STA) ||
                (interested == 3 && devices[i]->scanType == SCAN_WIFI_AP)) {
                    ++count;
            }
            snprintf(strVal, sizeof(strVal), "%d", count);
            attribute_value = strVal;
        }
    }
    wendigo_scene_status_add_attribute(app, attribute_name, attribute_value);
    FURI_LOG_T(WENDIGO_TAG, "End process_and_display_status_attribute()");
}

/** Parse a Wendigo packet representing a Bluetooth device.
 *  This function, and packet type, caters for both BT Classic
 *  and BT Low Energy devices. Creates a new wendigo_device and
 *  passes it to wendigo_add_device().
 */
uint16_t parseBufferBluetooth(WendigoApp *app, uint8_t *packet, uint16_t packetLen) {
    FURI_LOG_T(WENDIGO_TAG, "Start parseBufferBluetooth");
    /* Sanity check - we should have at least WENDIGO_OFFSET_BT_COD_LEN + PREAMBLE_LEN bytes */
    if (packetLen < (WENDIGO_OFFSET_BT_COD_LEN + PREAMBLE_LEN)) {
        char msg[98];
        snprintf(msg, sizeof(msg), "Bluetooth packet too short even before considering BDName, EIR and CoD. Expected %d, actual %d.",
                                    (WENDIGO_OFFSET_BT_COD_LEN + PREAMBLE_LEN), packetLen);
        wendigo_log_with_packet(MSG_WARN, msg, packet, packetLen);
        return packetLen;
    }
    wendigo_device *dev = malloc(sizeof(wendigo_device));
    if (dev == NULL) {
        // TODO: Panic. For now just skip the device
        return packetLen;
    }
    dev->radio.bluetooth.bdname = NULL;
    dev->radio.bluetooth.eir = NULL;
    dev->radio.bluetooth.bt_services.known_services = NULL;
    dev->radio.bluetooth.bt_services.service_uuids = NULL;
    dev->radio.bluetooth.cod_str = NULL;
    dev->view = NULL;
    /* Temporary variables for attributes that require transformation */
    uint8_t tagged;
    /* Copy fixed-byte members */
    memcpy(&(dev->radio.bluetooth.bdname_len),
        packet + WENDIGO_OFFSET_BT_BDNAME_LEN, sizeof(uint8_t));
    memcpy(&(dev->radio.bluetooth.eir_len), packet + WENDIGO_OFFSET_BT_EIR_LEN,
        sizeof(uint8_t));
    memcpy(&(dev->rssi), packet + WENDIGO_OFFSET_BT_RSSI, sizeof(int16_t));
    memcpy(&(dev->radio.bluetooth.cod), packet + WENDIGO_OFFSET_BT_COD,
        sizeof(uint32_t));
    memcpy(dev->mac, packet + WENDIGO_OFFSET_BT_BDA, MAC_BYTES);
    memcpy(&(dev->scanType), packet + WENDIGO_OFFSET_BT_SCANTYPE, sizeof(uint8_t));
    memcpy(&tagged, packet + WENDIGO_OFFSET_BT_TAGGED, sizeof(uint8_t));
    dev->tagged = (tagged == 1);
    /* Ignore lastSeen */
    // memcpy(&(dev->lastSeen.tv_sec), buffer + WENDIGO_OFFSET_BT_LASTSEEN,
    // sizeof(int64_t));
    memcpy(&(dev->radio.bluetooth.bt_services.num_services),
        packet + WENDIGO_OFFSET_BT_NUM_SERVICES, sizeof(uint8_t));
    memcpy(&(dev->radio.bluetooth.bt_services.known_services_len),
        packet + WENDIGO_OFFSET_BT_KNOWN_SERVICES_LEN, sizeof(uint8_t));
    uint8_t cod_len;
    memcpy(&cod_len, packet + WENDIGO_OFFSET_BT_COD_LEN, sizeof(uint8_t));
    /* Do we have a bdname? */
    uint16_t index = WENDIGO_OFFSET_BT_BDNAME;
    /* Rudimentary buffer capacity check - Is the buffer large enough to hold the
     * specified bdname and packet terminator? */
    if (dev->radio.bluetooth.bdname_len > 0 &&
            packetLen >= (WENDIGO_OFFSET_BT_BDNAME + dev->radio.bluetooth.bdname_len + PREAMBLE_LEN - 1)) {
        dev->radio.bluetooth.bdname = malloc(sizeof(char) * (dev->radio.bluetooth.bdname_len + 1));
        if (dev->radio.bluetooth.bdname == NULL) {
            /* Can't store bdname so set bdname_len to 0 */
            dev->radio.bluetooth.bdname_len = 0;
        } else {
            memcpy(dev->radio.bluetooth.bdname, packet + WENDIGO_OFFSET_BT_BDNAME,
                dev->radio.bluetooth.bdname_len);
            dev->radio.bluetooth.bdname[dev->radio.bluetooth.bdname_len] = '\0';
        }
        /* Move index past bdname */
        index += dev->radio.bluetooth.bdname_len;
    }
    /* EIR? */
    if (dev->radio.bluetooth.eir_len > 0 &&
            packetLen >= (index + dev->radio.bluetooth.eir_len + PREAMBLE_LEN - 1)) {
        dev->radio.bluetooth.eir = malloc(sizeof(uint8_t) * dev->radio.bluetooth.eir_len);
        if (dev->radio.bluetooth.eir == NULL) {
            dev->radio.bluetooth.eir_len = 0;
        } else {
            memcpy(dev->radio.bluetooth.eir, packet + index, dev->radio.bluetooth.eir_len);
        }
        index += dev->radio.bluetooth.eir_len;
    }
    /* Class of Device? */
    if (cod_len > 0 && packetLen >= (index + cod_len + PREAMBLE_LEN - 1)) {
        dev->radio.bluetooth.cod_str = malloc(sizeof(char) * (cod_len + 1));
        if (dev->radio.bluetooth.cod_str != NULL) {
            memcpy(dev->radio.bluetooth.cod_str, packet + index, cod_len);
            dev->radio.bluetooth.cod_str[cod_len] = '\0';
        }
        index += cod_len; /* Cater for the trailing '\0' */
    }
    // TODO: Services to go here

    /* Hopefully `index` now points to the packet terminator (unless scanning has
       stopped, then all bets are off) */
    if (app->is_scanning && memcmp(PACKET_TERM, packet + index, PREAMBLE_LEN)) {
        char msg[62];
        snprintf(msg, sizeof(msg), "Bluetooth packet terminator expected at index %d, not found.", index);
        wendigo_log_with_packet(MSG_WARN, msg, packet, packetLen);
    }

    /* Add or update the device in devices[] - No longer need to check
        whether we're adding or updating, the add/update functions will call
        each other if required. */
    wendigo_add_device(app, dev);
    /* Clean up memory */
    wendigo_free_device(dev);
    FURI_LOG_T(WENDIGO_TAG, "End parseBufferBluetooth()");
    return index + PREAMBLE_LEN;
}

/** Parse a Wendigo packet representing an Access Point.
 *  Creates a new wendigo_device and passes it to wendigo_add_device().
 *  This function does not manipulate the contents of the packet, and
 *  does not read the buffer.
 */
uint16_t parseBufferWifiAp(WendigoApp *app, uint8_t *packet, uint16_t packetLen) {
    FURI_LOG_T(WENDIGO_TAG, "Start parseBufferWifiAp()");
    /* Check the packet is a reasonable size */
    uint16_t expectedLen = WENDIGO_OFFSET_AP_SSID + PREAMBLE_LEN;
    if (packetLen < expectedLen) {
        char shortMsg[56];
        snprintf(shortMsg, sizeof(shortMsg),
            "AP packet too short, expected %d actual %d. Skipping.",
            expectedLen, packetLen);
        // Popup disabled while debugging device list wendigo_display_popup(app, "AP
        // too short", shortMsg);
        wendigo_log_with_packet(MSG_ERROR, shortMsg, packet, packetLen);
        FURI_LOG_T(WENDIGO_TAG, "End parseBufferWifiAp() - Packet too short");
        return packetLen;
    }
    /* Retrieve stations_count and ssid_len from the packet for further validation */
    uint8_t sta_count;
    uint8_t ssid_len;
    memcpy(&ssid_len, packet + WENDIGO_OFFSET_AP_SSID_LEN, sizeof(uint8_t));
    memcpy(&sta_count, packet + WENDIGO_OFFSET_AP_STA_COUNT, sizeof(uint8_t));
    /* Now we have stations_count and ssid_len we know exactly how big the packet should be */
    expectedLen = WENDIGO_OFFSET_AP_SSID + ssid_len + (MAC_BYTES * sta_count) + PREAMBLE_LEN;
    if (packetLen < expectedLen) {
        /* Packet is too short - Likely reflects a corrupted packet with the wrong
            byte in stations_count. Log and display the error and free `dev`.
        */
        char shortMsg[77];
        snprintf(shortMsg, sizeof(shortMsg),
            "Packet's stations_count requires packet of size %d, actual %d. "
            "Skipping.",
            expectedLen, packetLen);
        wendigo_log_with_packet(MSG_ERROR, shortMsg, packet, packetLen);
        // Popup disabled while debugging device list wendigo_display_popup(app, "AP
        // too short for STAtions", shortMsg);
        FURI_LOG_T(WENDIGO_TAG, "End parseBufferWifiAp() - Packet too short for stations");
        return packetLen;
    }
    wendigo_device *dev = malloc(sizeof(wendigo_device));
    if (dev == NULL) {
        return packetLen;
    }
    /* Initialise pointers */
    dev->view = NULL;
    dev->radio.ap.stations = NULL;
    bzero(dev->radio.ap.ssid, MAX_SSID_LEN + 1);
    /* Temporary variables for elements that need to be transformed */
    uint8_t tagged;
    /* Copy fixed-length attributes from the packet */
    memcpy(&(dev->scanType), packet + WENDIGO_OFFSET_WIFI_SCANTYPE,
        sizeof(uint8_t));
    memcpy(dev->mac, packet + WENDIGO_OFFSET_WIFI_MAC, MAC_BYTES);
    memcpy(&(dev->radio.ap.channel), packet + WENDIGO_OFFSET_WIFI_CHANNEL, sizeof(uint8_t));
    memcpy(&(dev->rssi), packet + WENDIGO_OFFSET_WIFI_RSSI, sizeof(int16_t));
    /* Ignore lastSeen */
    // memcpy(&(dev->lastSeen.tv_sec), buffer + WENDIGO_OFFSET_WIFI_LASTSEEN,
    // sizeof(int64_t));
    memcpy(&tagged, packet + WENDIGO_OFFSET_WIFI_TAGGED, sizeof(uint8_t));
    dev->tagged = (tagged == 1);
    dev->radio.ap.stations_count = sta_count;
    memcpy(dev->radio.ap.ssid, packet + WENDIGO_OFFSET_AP_SSID, ssid_len);

    /* Retrieve stations_count MAC addresses */
    uint8_t buffIndex = WENDIGO_OFFSET_AP_SSID + ssid_len;
    uint8_t **stations = NULL;
    if (dev->radio.ap.stations_count > 0) {
        stations = malloc(sizeof(uint8_t *) * dev->radio.ap.stations_count);
        if (stations == NULL) {
            free(dev);
            FURI_LOG_T(WENDIGO_TAG, "End parseBufferWifiAp() - Unable to malloc() stations[]");
            return packetLen;
        }
        for (uint8_t staIdx = 0; staIdx < dev->radio.ap.stations_count; ++staIdx) {
            stations[staIdx] = malloc(MAC_BYTES);
            if (stations[staIdx] == NULL) {
                buffIndex += MAC_BYTES;
                continue; /* Progress to the next station... Who knows, maybe it'll work */
            }
            memcpy(stations[staIdx], packet + buffIndex, MAC_BYTES);
            buffIndex += MAC_BYTES;
        }
    }
    /* buffIndex should now point to the packet terminator */
    if (memcmp(packet + buffIndex, PACKET_TERM, PREAMBLE_LEN)) {
        char bytesFound[3 * PREAMBLE_LEN];
        char popupMsg[3 * PREAMBLE_LEN + 30];
        bytes_to_string(packet + buffIndex, PREAMBLE_LEN, bytesFound);
        snprintf(popupMsg, sizeof(popupMsg), "Expected end of packet, found %s",
            bytesFound);
        popupMsg[3 * PREAMBLE_LEN + 29] = '\0';
        // Popup disabled while debugging device list wendigo_display_popup(app, "AP
        // Packet Error", popupMsg);
        wendigo_log_with_packet(MSG_ERROR, popupMsg, packet, packetLen);
        if (stations != NULL) {
            for (uint8_t i = 0; i < dev->radio.ap.stations_count; ++i) {
                if (stations[i] != NULL) {
                    free(stations[i]);
                }
            }
            free(stations);
        }
    } else {
        if (dev->radio.ap.stations_count > 0) {
            dev->radio.ap.stations = stations;
        }
        wendigo_add_device(app, dev);
    }
    wendigo_free_device(dev);
    FURI_LOG_T(WENDIGO_TAG, "End parseBufferWifiAp()");
    return packetLen;
}

uint16_t parseBufferWifiSta(WendigoApp *app, uint8_t *packet, uint16_t packetLen) {
    FURI_LOG_T(WENDIGO_TAG, "Start parseBufferWifiSta()");
    uint8_t expectedLen = WENDIGO_OFFSET_STA_AP_SSID + PREAMBLE_LEN;
    if (packetLen < expectedLen) {
        /* Packet is too short - Log the issue along with the current packet */
        char shortMsg[60];
        snprintf(shortMsg, sizeof(shortMsg),
            "STA packet too short: Expected %d, actual %d. Skipping...",
            expectedLen, packetLen);
        wendigo_log_with_packet(MSG_ERROR, shortMsg, packet, packetLen);
        /* Also display a popup */
        // Popup disabled while debugging device list wendigo_display_popup(app,
        // "STA packet too short", shortMsg);
        FURI_LOG_T(WENDIGO_TAG, "End parseBufferWifiSta() - Short packet");
        return packetLen;
    }
    /* Get ssid_len to validate the full packet size */
    uint8_t ap_ssid_len;
    memcpy(&ap_ssid_len, packet + WENDIGO_OFFSET_STA_AP_SSID_LEN, sizeof(uint8_t));
    expectedLen += ssid_len;
    if (packetLen < expectedLen) {
        /* Packet too short - Log and return */
        char shortMsg[57];
        snprintf(shortMsg, sizeof(shortMsg),
            "STA packet too short for SSID: Expected %d, actual %d.",
            expectedLen, packetLen);
        wendigo_log_with_packet(MSG_ERROR, shortMsg, packet, packetLen);
        FURI_LOG_T(WENDIGO_TAG, "End parseBufferWifiSta() - Packet too short for SSID");
        return packetLen;
    }
    wendigo_device *dev = malloc(sizeof(wendigo_device));
    if (dev == NULL) {
        return packetLen;
    }
    /* Initialise all pointers */
    dev->view = NULL;
    memcpy(dev->radio.sta.apMac, nullMac, MAC_BYTES);
    /* Temp variables for attributes that need to be transformed */
    uint8_t tagged;
    /* Copy fixed-length attributes */
    memcpy(&(dev->scanType), packet + WENDIGO_OFFSET_WIFI_SCANTYPE, sizeof(uint8_t));
    memcpy(dev->mac, packet + WENDIGO_OFFSET_WIFI_MAC, MAC_BYTES);
    memcpy(&(dev->radio.sta.channel), packet + WENDIGO_OFFSET_WIFI_CHANNEL, sizeof(uint8_t));
    memcpy(&(dev->rssi), packet + WENDIGO_OFFSET_WIFI_RSSI, sizeof(int16_t));
    /* Ignore lastSeen */
    // memcpy(&(dev->lastSeen.tv_sec), buffer + WENDIGO_OFFSET_WIFI_LASTSEEN,
    // sizeof(int64_t));
    memcpy(&tagged, packet + WENDIGO_OFFSET_WIFI_TAGGED, sizeof(uint8_t));
    dev->tagged = (tagged == 1);
    memcpy(dev->radio.sta.apMac, packet + WENDIGO_OFFSET_STA_AP_MAC, MAC_BYTES);
    char ap_ssid[MAX_SSID_LEN + 1];
    bzero(ap_ssid, MAX_SSID_LEN + 1);
    memcpy(ap_ssid, packet + WENDIGO_OFFSET_STA_AP_SSID, ap_ssid_len);
    ap_ssid[ap_ssid_len] = '\0';
    /* Do I want to do anything with ap_ssid? */
    /* We should find the packet terminator after the SSID */
    if (memcmp(PACKET_TERM, packet + WENDIGO_OFFSET_STA_AP_SSID + ap_ssid_len, PREAMBLE_LEN)) {
        // Popup disabled while debugging device list        wendigo_display_popup(
        //            app, "STA Packet", "STA Packet terminator not found where
        //            expected.");
        /* Log the packet so the cause can hopefully be found */
        wendigo_log_with_packet(
            MSG_ERROR, "STA packet terminator not found where expected, skipping.",
            packet, packetLen);
    } else {
        wendigo_add_device(app, dev);
    }
    wendigo_free_device(dev);
    FURI_LOG_T(WENDIGO_TAG, "End parseBufferWifiSta()");
    return packetLen;
}

/** Parse a version packet and display both Flipper- and ESP32-Wendigo versions.
 * Returns the number of bytes consumed from the buffer - DOES NOT remove
 * consumed bytes, this must be handled by the calling function.
 */
uint16_t parseBufferVersion(WendigoApp *app, uint8_t *packet, uint16_t packetLen) {
    UNUSED(app);
    FURI_LOG_T(WENDIGO_TAG, "Start parseBufferVersion()");
    /* Find the end-of-packet sequence to determine version string length */
    uint16_t endSeq = packetLen - PREAMBLE_LEN; /* To reach first byte in seq */
    /* Add space for Flipper-Wendigo version information. */
    uint16_t messageLen = endSeq + 26 + strlen(FLIPPER_WENDIGO_VERSION);
    char *versionStr = realloc(wendigo_popup_text, sizeof(char) * messageLen);
    if (versionStr == NULL) {
        // TODO: Panic
        // For now just consume this message
        return endSeq + PREAMBLE_LEN;
    }
    wendigo_popup_text = versionStr;
    snprintf(wendigo_popup_text, messageLen, "Flipper-Wendigo v%s\n ESP32-", FLIPPER_WENDIGO_VERSION);
    memcpy(wendigo_popup_text + 25 + strlen(FLIPPER_WENDIGO_VERSION), packet, endSeq);
    wendigo_popup_text[messageLen - 1] = '\0'; /* Just in case */
    wendigo_display_popup(app, "Wendigo Version", wendigo_popup_text);
    FURI_LOG_T(WENDIGO_TAG, "End parseBufferVersion()");
    return packetLen;
}

uint16_t parseBufferChannels(WendigoApp *app, uint8_t *packet, uint16_t packetLen) {
    UNUSED(app);
    FURI_LOG_T(WENDIGO_TAG, "Start parseBufferChannels()");
    uint8_t channels_count;
    uint8_t *channels = NULL;
    memcpy(&channels_count, packet + WENDIGO_OFFSET_CHANNEL_COUNT, sizeof(uint8_t));
    if (channels_count > 0) {
        channels = malloc(channels_count);
        if (channels != NULL) {
            memcpy(channels, packet + WENDIGO_OFFSET_CHANNELS, channels_count);
        }
    }
    if (memcmp(PACKET_TERM, packet + WENDIGO_OFFSET_CHANNELS + channels_count, PREAMBLE_LEN)) {
        //        wendigo_display_popup(app, "Channel Packet", "Channel packet is
        //        unexpected length");
        wendigo_log_with_packet(MSG_ERROR, "Channels packet terminator not found where expected", packet, packetLen);
        FURI_LOG_T(WENDIGO_TAG, "End parseBufferChannels() - Terminator not found");
        return packetLen;
    }
    // TODO: Do something with channels
    FURI_LOG_T(WENDIGO_TAG, "End parseBufferChannels()");
    return packetLen;
}

/** Parse a status packet and display in the status view.
 * This function requires that Wendigo_AppViewStatus be the
 * currently-displayed view (otherwise the packet is discarded).
 */
uint16_t parseBufferStatus(WendigoApp *app, uint8_t *packet, uint16_t packetLen) {
    FURI_LOG_T(WENDIGO_TAG, "Start parseBufferStatus()");
    /* Ignore the packet if the status scene isn't displayed */
    if (app->current_view != WendigoAppViewStatus) {
        return packetLen;
    }
    wendigo_scene_status_begin_layout(app);
    // TODO: Document the packet somewhere. Less straightforward than the
    // Bluetooth packet because it's a dynamic size, but it needs *something*
    uint8_t attribute_count;
    uint8_t attribute_name_len;
    uint8_t attribute_value_len;
    char *attribute_name = NULL;
    char *attribute_value = NULL;
    uint16_t offset = PREAMBLE_LEN;
    memcpy(&attribute_count, packet + offset, sizeof(uint8_t));
    ++offset;
    for (uint8_t i = 0; i < attribute_count; ++i) {
        /* Parse attribute name length */
        memcpy(&attribute_name_len, packet + offset, sizeof(uint8_t));
        if (attribute_name_len == 0) {
            wendigo_log_with_packet(MSG_ERROR, "Status packet contained an attribute of length 0, skipping.", packet, packetLen);
            return packetLen;
        }
        ++offset;
        /* Name */
        attribute_name = malloc(sizeof(char) * (attribute_name_len + 1));
        if (attribute_name == NULL) {
            char msg[51];
            snprintf(msg, sizeof(msg), "Failed to allocate %d bytes for status attribute.", attribute_name_len + 1);
            wendigo_log_with_packet(MSG_ERROR, msg, packet, packetLen);
            return packetLen;
        }
        memcpy(attribute_name, packet + offset, attribute_name_len);
        attribute_name[attribute_name_len] = '\0';
        offset += attribute_name_len;
        /* Attribute value length */
        memcpy(&attribute_value_len, packet + offset, sizeof(uint8_t));
        ++offset;
        /* It's valid for this to have a length of 0 - attribute_value will be "" */
        attribute_value = malloc(sizeof(char) * (attribute_value_len + 1));
        if (attribute_value == NULL) {
            char msg[50];
            snprintf(msg, sizeof(msg), "Failed to allocate %d bytes for attribute value.", attribute_value_len + 1);
            wendigo_log_with_packet(MSG_ERROR, msg, packet, packetLen);
            free(attribute_name);
            return packetLen;
        }
        memcpy(attribute_value, packet + offset, attribute_value_len);
        attribute_value[attribute_value_len] = '\0';
        offset += attribute_value_len;
        /* Send the attribute off for validation and display */
        process_and_display_status_attribute(app, attribute_name, attribute_value);
        free(attribute_name);
        free(attribute_value);
    }
    wendigo_scene_status_finish_layout(app);

    /* buffer + offset should now point to the end of packet sequence */
    if (memcmp(PACKET_TERM, packet + offset, PREAMBLE_LEN)) {
        wendigo_log_with_packet(MSG_WARN, "Status packet terminator not found where expected.", packet, packetLen);
        return packetLen;
    }
    FURI_LOG_T(WENDIGO_TAG, "End parseBufferStatus()");
    return offset + PREAMBLE_LEN;
}

/** When the end of a packet is reached this function is called to parse the
 *  buffer and take the appropriate action. We expect to see one of the
 * packet preambles defined in wendigo_common_defs.c
 *  The end of packet is marked by 0xAA,0xBB,0xCC,0xDD.
 */
void parsePacket(WendigoApp *app, uint8_t *packet, uint16_t packetLen) {
    FURI_LOG_T(WENDIGO_TAG, "Begin parsePacket(len: %d)", packetLen);
    /* Update buffer last received time */
    app->last_packet = furi_hal_rtc_get_timestamp();

    /* Development: Dump packet for inspection */
    wendigo_log_with_packet(MSG_DEBUG, "parsePacket() received packet", packet, packetLen);

    if (packetLen < (2 * PREAMBLE_LEN)) {
        wendigo_log_with_packet(MSG_WARN, "parsePacket(): Packet too short:", packet, packetLen);
        return;
    }

    /* Pass packet off to the relevant packet parser */
    if (!memcmp(PREAMBLE_BT_BLE, packet, PREAMBLE_LEN)) {
        parseBufferBluetooth(app, packet, packetLen);
    } else if (!memcmp(PREAMBLE_WIFI_AP, packet, PREAMBLE_LEN)) {
        parseBufferWifiAp(app, packet, packetLen);
    } else if (!memcmp(PREAMBLE_WIFI_STA, packet, PREAMBLE_LEN)) {
        parseBufferWifiSta(app, packet, packetLen);
    } else if (!memcmp(PREAMBLE_STATUS, packet, PREAMBLE_LEN)) {
        parseBufferStatus(app, packet, packetLen);
    } else if (!memcmp(PREAMBLE_VER, packet, PREAMBLE_LEN)) {
        parseBufferVersion(app, packet, packetLen);
    } else if (!memcmp(PREAMBLE_CHANNELS, packet, PREAMBLE_LEN)) {
        parseBufferChannels(app, packet, packetLen);
    } else {
        wendigo_log_with_packet(MSG_WARN, "Packet doesn't have a valid preamble", packet, packetLen);
    }
    FURI_LOG_T(WENDIGO_TAG, "End parsePacket()");
}

/** Callback invoked when UART data is received. When an end-of-packet sequence
 *  is received the buffer is sent to the appropriate handler for display. At the
 *  time of writing a similar callback exists in wendigo_scene_console_output,
 *  that is being retained through initial stages of development because it
 *  provides a useful way to run ad-hoc tests from Flipper. Eventually the entire
 *  scene wendigo_scene_console_output will likely be removed, until then
 *  collision can be avoided by limiting its use, ensuring that
 *  wendigo_uart_set_handle_rx_data_cb() is called after using the console to
 *  re-establish this function as the UART callback.
 */
void wendigo_scan_handle_rx_data_cb(uint8_t *buf, size_t len, void *context) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scan_handle_rx_data_cb()");
    furi_assert(context);
    WendigoApp *app = context;

    /* Extend the buffer if necessary */
    if (bufferLen + len >= bufferCap) {
        /* Extend it by the larger of len and BUFFER_INC_CAPACITY_BY bytes to avoid constant realloc()s */
        uint8_t increase_by = (len > BUFFER_INC_CAPACITY_BY) ? len : BUFFER_INC_CAPACITY_BY;
        uint16_t newCapacity = bufferCap + increase_by;
        FURI_LOG_D(WENDIGO_TAG, "Expanding buffer from %d to %d", bufferCap, newCapacity);
        /* Will this exceed the maximum capacity? */
        if (newCapacity > BUFFER_MAX_SIZE) {
            // TODO: Come up with an alternate way to do this
            FURI_LOG_D(WENDIGO_TAG, "Buffer too large");
            return;
            /* Remove increase_by bytes from beginning of buffer[] instead */
            uint8_t *new_start = buffer + increase_by;
            memcpy(buffer, new_start, bufferLen - increase_by);
            memset(buffer + bufferLen - increase_by, '\0', increase_by);
            bufferLen -= increase_by;
        } else {
            uint8_t *newBuffer = realloc(buffer, sizeof(uint8_t *) * newCapacity); // Behaves like malloc() when buffer==NULL
            if (newBuffer == NULL) {
                /* Out of memory */
                // TODO: Panic
                return;
            }
            buffer = newBuffer;
            bufferCap = newCapacity;
        }
    }
    memcpy(buffer + bufferLen, buf, len);
    bufferLen += len;

    /* Parse any complete packets we have received */
    uint8_t *packet;
    uint16_t packetLen;
    uint16_t startIdx = start_of_packet(buffer, bufferLen);
    uint16_t endIdx = end_of_packet(buffer, bufferLen);
    if (endIdx < startIdx) {
        /* A corrupted packet has an end sequence but not a start sequence - NULL it
            and find the next end sequence */
        memset(buffer, 0, endIdx + 1);
        endIdx = end_of_packet(buffer, bufferLen);
    }
    while (startIdx < endIdx && endIdx < bufferLen) {
        /* We have a complete packet - extract it for parsing */
        packetLen = endIdx - startIdx + 1;
        packet = buffer + startIdx;

        parsePacket(app, packet, packetLen);

        /* Remove this packet from the buffer and look for another */
        memset(buffer + startIdx, 0, packetLen);
        /* If startIdx > 0 all non-NULL bytes before startIdx are spurious - remove them */
        if (startIdx > 0) {
            FURI_LOG_D(WENDIGO_TAG, "startIdx %d, NULLING previous bytes\n",
                startIdx);
            memset(buffer, 0, startIdx);
        }
        startIdx = start_of_packet(buffer, bufferLen);
        endIdx = end_of_packet(buffer, bufferLen);
    }
    /* Have we been able to empty the buffer? */
    if (startIdx == bufferLen && endIdx == bufferLen) {
        /* Not having a start or end packet sequence is a start - Check for all NULL
        * bytes in buffer */
        bool bytesFound = false;
        for (uint16_t i = 0; i < bufferLen && !bytesFound; ++i) {
            if (buffer[i] != 0) {
                bytesFound = true;
            }
        }
        if (!bytesFound) {
            FURI_LOG_D("wendigo_scan_handle_rx_data_cb()",
                "Buffer is empty, resetting bufferLen");
            bufferLen = 0;
        }
    }
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scan_handle_rx_data_cb()");
}

void wendigo_free_uart_buffer() {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_free_uart_buffer()");
    if (bufferCap > 0) {
        free(buffer);
        bufferLen = 0;
        bufferCap = 0;
        buffer = NULL;
    }
    /* Used by the version command */
    if (wendigo_popup_text != NULL) {
        free(wendigo_popup_text);
        wendigo_popup_text = NULL;
    }
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_free_uart_buffer()");
}

void wendigo_log(MsgType logType, char *message) {
    switch (logType) {
        case MSG_ERROR:
            FURI_LOG_E(WENDIGO_TAG, message);
            break;
        case MSG_WARN:
            FURI_LOG_W(WENDIGO_TAG, message);
            break;
        case MSG_INFO:
            FURI_LOG_I(WENDIGO_TAG, message);
            break;
        case MSG_DEBUG:
            FURI_LOG_D(WENDIGO_TAG, message);
            break;
        case MSG_TRACE:
            FURI_LOG_T(WENDIGO_TAG, message);
            break;
        default:
            break;
    }
}

void wendigo_log_with_packet(MsgType logType, char *message, uint8_t *packet,
                            uint16_t packet_size) {
    if (packet == NULL || packet_size == 0) {
        return;
    }
    uint16_t messageLen = 3 * packet_size;
    if (message != NULL) {
        messageLen += strlen(message) + 1;
    }
    char *finalMessage = malloc(sizeof(char) * messageLen);
    if (finalMessage != NULL) {
        memcpy(finalMessage, message, strlen(message));
        finalMessage[strlen(message)] = '\n';
        bytes_to_string(packet, packet_size, finalMessage + strlen(message) + 1);
    }
    /* Just in case my counting is out */
    finalMessage[messageLen - 1] = '\0';
    wendigo_log(logType, finalMessage);
    free(finalMessage);
}
