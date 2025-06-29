#include "wendigo_scan.h"

uint8_t *buffer = NULL;
uint16_t bufferLen = 0; // 65535 should be plenty of length
uint16_t bufferCap = 0; // Buffer capacity - I don't want to allocate 65kb, but don't want to constantly realloc
char *wendigo_popup_text = NULL; // I suspect the popup text is going out of scope when declared at function scope

/* Internal function declarations */
uint16_t custom_device_index(wendigo_device *dev, wendigo_device **array, uint16_t array_count);

/* Device caches */
wendigo_device **devices = NULL;
wendigo_device **selected_devices = NULL;
uint16_t devices_count = 0;
uint16_t selected_devices_count = 0;
uint16_t devices_capacity = 0;
uint16_t selected_devices_capacity = 0;

/* Packet identifiers */
uint8_t PREAMBLE_LEN = 8;
uint8_t PREAMBLE_BT_BLE[]   = {0xFF, 0xFF, 0xFF, 0xFF, 0xAA, 0xAA, 0xAA, 0xAA};
uint8_t PREAMBLE_WIFI_AP[]  = {0x99, 0x99, 0x99, 0x99, 0x11, 0x11, 0x11, 0x11};
uint8_t PREAMBLE_WIFI_STA[] = {0x99, 0x99, 0x99, 0x99, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t PREAMBLE_STATUS[]   = {0xEE, 0xEE, 0xEE, 0xEE, 0xBB, 0xBB, 0xBB, 0xBB};
uint8_t PREAMBLE_VER[]      = {'W', 'e', 'n', 'd', 'i', 'g', 'o', ' '};
uint8_t PACKET_TERM[]       = {0xAA, 0xAA, 0xAA, 0xAA, 0xFF, 0xFF, 0xFF, 0xFF};

/* How much will we increase bt_devices[] by when additional space is needed? */
#define INC_BT_DEVICE_CAPACITY_BY   10

/** Search for the start-of-packet marker in the specified byte array
 *  This function is used during parsing to skip past any extraneous bytes
 *  that are sent prior to the start-of-packet sequence.
 *  Returns `size` if not found, otherwise returns the index of the first byte of the sequence.
 */
uint16_t start_of_packet(uint8_t *bytes, uint16_t size) {
    uint16_t result = 0;
    if (bytes == NULL) { return size; }
    for (; result + PREAMBLE_LEN <= size && memcmp(bytes + result, PREAMBLE_BT_BLE, PREAMBLE_LEN) &&
            memcmp(bytes + result, PREAMBLE_WIFI_AP, PREAMBLE_LEN) &&
            memcmp(bytes + result, PREAMBLE_WIFI_STA, PREAMBLE_LEN) &&
            memcmp(bytes + result, PREAMBLE_STATUS, PREAMBLE_LEN) &&
            memcmp(bytes + result, PREAMBLE_VER, PREAMBLE_LEN); ++result) { }
    return result;
}

/** Search for the end-of-packet marker in the specified byte array
 *  A packet is terminated by a sequence of 4 0xAA and 4 0xFF,
 *  returns the index of the last byte in this sequence, or the
 *  specified `size` if not found.
 */
uint16_t end_of_packet(uint8_t *theBytes, uint16_t size) {
    uint16_t result = PREAMBLE_LEN - 1; /* Start by checking bytes [0] through [7] */
    if (theBytes == NULL) { return size; }
    for (; result < size && memcmp(theBytes + result - PREAMBLE_LEN + 1, PACKET_TERM, PREAMBLE_LEN); ++result) { }
    return result;
}

/** Consume `bytes` bytes from `buffer`.
 *  This function does not alter buffer capacity, but updates buffer and bufferLen
 *  to remove the specified number of bytes from the beginning of the buffer.
 * If the specified number of bytes is greater than the current buffer length then
 * bufferLen bytes are removed from the buffer, emptying it.
 */
void consumeBufferBytes(uint16_t bytes) {
    if (bytes == 0 || buffer == NULL || bufferLen == 0) {
        // That was a bit of a waste
        return;
    }
    /* Ensure we stay within buffer's allocated memory */
    if (bytes > bufferLen) {
        bytes = bufferLen;
    }
    /* Remove bytes from the beginning of the buffer */
    memset(buffer, '\0', bytes);
    /* Copy memory into buffer from buffer+bytes to buffer+bufferLen.
       This should be OK when the buffer is exhausted because it copies 0 bytes */
    memcpy(buffer, buffer + bytes, bufferLen - bytes);
    /* Null what remains */
    memset(buffer + bufferLen - bytes, '\0', bytes);
    /* Buffer is now bufferLen - bytes */
    bufferLen -= bytes;
}

/* Returns the index of the first occurrence of `\n` in `theString`, searching the first `size` characters.
   Returns size if not found (which is outside the array bounds).
   TODO: This function is a candidate for deletion now that packets use an alternate packet terminator */
uint16_t bytes_contains_newline(uint8_t *theBytes, uint16_t size) {
    if (theBytes == NULL) { return size; }
    uint16_t result = 0;
    for (; result < size && theBytes[result] != '\n'; ++result) { }
    return result;
}

/* Returns the index of the first occurrence of '\n' in `theString`, or strlen(theString) if not found.
   This function requires `theString` to be null terminated.
   If theString is NULL or if strlen(theString) > 0xFFFF (65535, MAX_UINT16) the function returns
   0xFFFF. If this value is returned and theString is not NULL it's a good indication the string is not
   NULL-terminated.
   TODO: This function is a candidate for deletion. */
uint16_t string_contains_newline(char *theString) {
    if (theString == NULL || strlen(theString) > 0xFFFF) { return 0xFFFF; }

    return bytes_contains_newline((uint8_t *)theString, strlen(theString));
}

/* Send the status command to ESP32 */
void wendigo_esp_status(WendigoApp *app) {
    char cmd[] = "s\n";
    wendigo_uart_tx(app->uart, (uint8_t *)cmd, strlen(cmd) + 1);
}

/* Send the version command to ESP32 */
void wendigo_esp_version(WendigoApp *app) {
    char cmd[] = "v\n";
    wendigo_uart_tx(app->uart, (uint8_t *)cmd, strlen(cmd) + 1);
}

/* Manage the tagged/selected status of the specified flipper_bt_device
   Maintains bt_selected_devices[] as needed */
bool wendigo_set_device_selected(wendigo_device *device, bool selected) {
    /* NULL device check */
    if (device == NULL) { return false; }
    if (selected && !device->tagged) {
        /* Add device to selected_devices[] */
        if (selected_devices_capacity == selected_devices_count) {
            /* Extend selected_devices[] */
            wendigo_device **new_devices = realloc(selected_devices, sizeof(wendigo_device *) * (selected_devices_capacity + 1));
            if (new_devices == NULL) {
                // TODO: Panic - Display an error popup or something
                return false;
            }
            selected_devices = new_devices;
            ++selected_devices_capacity;
        }
        selected_devices[selected_devices_count++] = device;
        // TODO: Sort
    } else if (device->tagged && !selected) {
        /* Remove device from selected_devices[] - custom_device_index() will safely handle any NULLs */
        /* Not using 16 bits because nobody's going to select 255 devices */
        uint8_t index = (uint8_t)custom_device_index(device, selected_devices, selected_devices_count);
        if (index < selected_devices_count) {
            /* Shuffle forward all subsequent devices, replacing device at `index` */
            for(++index; index < selected_devices_count; ++index) {
                selected_devices[index - 1] = selected_devices[index];
            }
            selected_devices[selected_devices_count--] = NULL;
        }
    }
    device->tagged = selected;
    return true;
}

/* Enable or disable Wendigo scanning on all interfaces, using app->interfaces
   to determine which radios should be enabled/disabled when starting to scan.
   This function is called by the UI handler (wendigo_scene_start) when scanning
   is started or stopped. app->interfaces is updated via the Setup menu.
   Because wendigo_uart_tx returns void there is no way to identify failure,
   so this function also returns void.
*/
void wendigo_set_scanning_active(WendigoApp *app, bool starting) {
    char cmd;
    uint8_t arg;
    const uint8_t CMD_LEN = 5; // e.g. "b 1\n\0"
    char cmdString[CMD_LEN];
    /* This flag will cause incomplete packets to be ignored */
    app->is_scanning = starting;
    for (int i = 0; i < IF_COUNT; ++i) {
        /* Set command */
        cmd = (i == IF_BLE) ? 'b' : (i == IF_BT_CLASSIC) ? 'h' : 'w';
        /* arg */
        arg = (starting && app->interfaces[i].active) ? 1 : 0;
        snprintf(cmdString, CMD_LEN, "%c %d\n", cmd, arg);
        wendigo_uart_tx(app->uart, (uint8_t *)cmdString, CMD_LEN);
    }
}

/* Returns the index into `array` of the device with MAC/BDA matching dev->mac.
   Returns `array_count` if the device was not found. */
uint16_t custom_device_index(wendigo_device *dev, wendigo_device **array, uint16_t array_count) {
    uint16_t result;
    if (dev == NULL || array == NULL) { return array_count; }
    for (result = 0; result < array_count && memcmp(dev->mac, array[result]->mac, MAC_BYTES); ++result) { }
    return result;
}

/* Returns the index into devices[] of the device with MAC/BDA matching dev->mac.
   Returns devices_count if the device was not found. A NULL value for `dev` is handled
   correctly by custom_device_index() */
uint16_t device_index(wendigo_device *dev) {
    return custom_device_index(dev, devices, devices_count);
}

/* Determines whether a device with the MAC/BDA dev->mac exists in devices[].
   A NULL value for `dev` is handled correctly at the top of the call stack,
   by custom_device_index(). */
bool device_exists(wendigo_device *dev) {
    return device_index(dev) < devices_count;
}

bool wendigo_add_bt_device(wendigo_device *dev, wendigo_device *new_device) {
    new_device->radio.bluetooth.cod = dev->radio.bluetooth.cod;
    if (dev->radio.bluetooth.cod_str != NULL) {
        new_device->radio.bluetooth.cod_str = malloc(sizeof(char) * (strlen(dev->radio.bluetooth.cod_str) + 1));
        if (new_device->radio.bluetooth.cod_str != NULL) {
            /* No need to handle allocation failure - we simply don't copy cod_str */
            strncpy(new_device->radio.bluetooth.cod_str, dev->radio.bluetooth.cod_str, strlen(dev->radio.bluetooth.cod_str));
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
            strncpy(new_device->radio.bluetooth.bdname, dev->radio.bluetooth.bdname, dev->radio.bluetooth.bdname_len);
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
            memcpy(new_device->radio.bluetooth.eir, dev->radio.bluetooth.eir, dev->radio.bluetooth.eir_len);
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
        /* known_services is an array of pointers - this is where we stop controlling memory allocation 
           so that bt_uuid's can be allocated a single time and reused.
           This function will copy the *pointers* in known_services[], but not the bt_uuid structs that
           they point to. */
        new_device->radio.bluetooth.bt_services.known_services =
            malloc(sizeof(bt_uuid *) * dev->radio.bluetooth.bt_services.known_services_len);
        if (new_device->radio.bluetooth.bt_services.known_services == NULL) {
            new_device->radio.bluetooth.bt_services.known_services_len = 0;
        } else {
            memcpy(new_device->radio.bluetooth.bt_services.known_services,
                dev->radio.bluetooth.bt_services.known_services,
                sizeof(bt_uuid *) * dev->radio.bluetooth.bt_services.known_services_len);
            // TODO: Despite the caveat above, it might be nice to validate that the bt_uuid's we're pointing to actually exist.
        }
    } else {
        new_device->radio.bluetooth.bt_services.known_services = NULL;
        new_device->radio.bluetooth.bt_services.known_services_len = 0;
    }
    return true;
}

bool wendigo_update_bt_device(wendigo_device *dev, wendigo_device *new_device) {
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
        if (new_name != NULL) { /* If allocation fails target's existing bdname and bdname_len are unmodified */
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
            new_device->radio.bluetooth.bt_services.num_services = dev->radio.bluetooth.bt_services.num_services;
        }
    }
    /* Known services */
    if (dev->radio.bluetooth.bt_services.known_services_len > 0 &&
            dev->radio.bluetooth.bt_services.known_services != NULL) {
        bt_uuid **new_known_svcs = realloc(new_device->radio.bluetooth.bt_services.known_services,
                        sizeof(bt_uuid *) * dev->radio.bluetooth.bt_services.known_services_len);
        if (new_known_svcs != NULL) {
            memcpy(new_known_svcs, dev->radio.bluetooth.bt_services.known_services,
                   sizeof(bt_uuid *) * dev->radio.bluetooth.bt_services.known_services_len);
            new_device->radio.bluetooth.bt_services.known_services = new_known_svcs;
            new_device->radio.bluetooth.bt_services.known_services_len =
                dev->radio.bluetooth.bt_services.known_services_len;
        }
    }
    return true;
}

/** Add the specified device to devices[], extending the length of devices[] if necessary.
 * If the specified device has a MAC/BDA which is already present in devices[] a new entry will
 * not be made, instead the element with the same MAC/BDA will be updated based on the specified device.
 * Returns true if the device was successfully added to (or updated in) devices[].
 * NOTE: The calling function may free the specified wendigo_device or any of its attributes
 * when this function returns. To minimise the likelihood of memory leaks this function will allocate
 * its own memory to hold the specified device and its attributes.
 */
bool wendigo_add_device(WendigoApp *app, wendigo_device *dev) {
    uint16_t idx = device_index(dev);
    if (idx < devices_count) {
        /* A device with the provided BDA already exists - Update that instead */
        return wendigo_update_device(app, dev);
    }
    /* Adding to devices - Increase capacity by an additional INC_BT_DEVICE_CAPACITY_BY if necessary */
    if (devices == NULL || devices_capacity == devices_count) {
        wendigo_device **new_devices = realloc(devices, sizeof(wendigo_device *) * (devices_capacity + INC_BT_DEVICE_CAPACITY_BY));
        if (new_devices == NULL) {
            /* Can't store the device */
            return false;
        }
        devices = new_devices;
        devices_capacity += INC_BT_DEVICE_CAPACITY_BY;
    }
    wendigo_device *new_device = malloc(sizeof(wendigo_device));
    if (new_device == NULL) {
        /* That's unfortunate */
        return false;
    }
    /* Copy common attributes */
    new_device->rssi = dev->rssi;
    new_device->scanType = dev->scanType;
    /* Don't copy tagged status - A new device is not tagged. */
    new_device->tagged = false;
    new_device->view = dev->view; /* Copy the view pointer if we have one, NULL if we don't */
    /* ESP32 doesn't know the real time/date so overwrite the lastSeen value.
       time_t is just another way of saying long long int, so casting is OK */
    new_device->lastSeen.tv_sec = (time_t)furi_hal_rtc_get_timestamp();
    /* Copy MAC/BDA */
    memcpy(new_device->mac, dev->mac, MAC_BYTES);
    /* Copy protocol-specific attributes */
    if (dev->scanType == SCAN_HCI || dev->scanType == SCAN_BLE) {
        wendigo_add_bt_device(dev, new_device);
    } else if (dev->scanType == SCAN_WIFI_AP) {
        //
    } else if (dev->scanType == SCAN_WIFI_STA) {
        //
    }

    devices[devices_count++] = new_device;

    /* If the device list scene is currently displayed, add the device to the UI */
    if (app->current_view == WendigoAppViewDeviceList) {
        wendigo_scene_device_list_update(app, new_device);
    }

    return true;
}

/** Locate the device in devices[] with the same MAC/BDA as `dev` and update the object based
 * on the contents of `dev`. If a device with the specified MAC/BDA does not exist a new device will
 * be added to devices[].
 * Returns true if the specified device has been updated (or a new device added) successfully.
 * NOTE: The calling function may free `dev` or any of its members immediately upon this function
 * returning. To minimise the likelihood of memory leaks, the device cache management functions
 * manage the allocation and free'ing of all components of the device cache *except for* bt_uuid
 * objects that form part of service descriptors.
 */
bool wendigo_update_device(WendigoApp *app, wendigo_device *dev) {
    uint16_t idx = device_index(dev);
    if (idx == devices_count) {
        /* Device doesn't exist in bt_devices[] - Add it instead */
        return wendigo_add_device(app, dev);
    }

    wendigo_device *target = devices[idx];
    /* Copy common attributes */
    target->rssi = dev->rssi;
    target->scanType = dev->scanType;
    /* Replace lastSeen - cast to long long int (aka time_t) */
    target->lastSeen.tv_sec = (time_t)furi_hal_rtc_get_timestamp();
    /* Copy protocol-specific attributes */
    if (dev->scanType == SCAN_HCI || dev->scanType == SCAN_BLE) {
        wendigo_update_bt_device(dev, target);
    } else if (dev->scanType == SCAN_WIFI_AP) {
        //
    } else if (dev->scanType == SCAN_WIFI_STA) {
        //
    }

    /* Update the device list if it's currently displayed */
    if (app->current_view == WendigoAppViewDeviceList) {
        wendigo_scene_device_list_update(app, target);
    } else if (app->current_view == WendigoAppViewDeviceDetail) { // && selectedDevice == bt_devices[idx]
        // TODO: Update existing view
    }

    return true;
}

/** Free all memory allocated to the specified device.
 * After free'ing its members this function will also free `dev` itself.
 * dev->view will not be touched by this function - It is allocated and freed in wendigo_scene_device_list.c.
 * NOTE: I've gone back and forth between setting attributes to NULL and 0 after freeing and not (because the
 * attributes aren't accessible after freeing `dev`), but have finally made a decision: Attributes representing
 * allocated memory ARE set to NULL (for pointers) and 0 (for counts) to reduce the likelihood that any orphan
 * pointers will attempt to use deallocated memory.
 */
void wendigo_free_device(wendigo_device *dev) {
    if (dev == NULL) {
        return;
    }
    switch (dev->scanType) {
        case SCAN_HCI:
        case SCAN_BLE:
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
                // TODO: After implementing BT services, ensure that the bt_uuid's are freed somewhere
                free(dev->radio.bluetooth.bt_services.known_services);
                dev->radio.bluetooth.bt_services.known_services = NULL;
                dev->radio.bluetooth.bt_services.known_services_len = 0;
            }
            break;
        case SCAN_WIFI_AP:
            if (dev->radio.ap.stations != NULL) {
                free(dev->radio.ap.stations);
                dev->radio.ap.stations = NULL;
                dev->radio.ap.stations_count = 0;
            }
            break;
        case SCAN_WIFI_STA:
            dev->radio.sta.ap = NULL;
            break;
        default:
            /* Nothing to do */
            break;
    }
    dev->view = NULL;
    free(dev);
}

/** Deallocates all memory allocated to the device cache.
 * This function deallocates all elements of the device cache, devices[] and selected_devices[].
 * These arrays are left in a coherent state, with the arrays set to NULL and their count & capacity
 * variables set to zero.
 */
void wendigo_free_devices() {
    /* Start by freeing selected_devices[] */
    if (selected_devices_capacity > 0 && selected_devices != NULL) {
        free(selected_devices); /* Elements will be freed when addressing bt_devices[] */
        selected_devices = NULL;
        selected_devices_count = 0;
        selected_devices_capacity = 0;
    }
    /* For devices[] we also want to free device attributes and the devices themselves */
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
}

/** During testing I bumped the ESP32 reset button and was irritated that Flipper's device
 * cache was working perfectly but status claimed there were no devices in the cache
 * despite displaying them). This function provides a location to validate and alter status
 * attributes prior to displaying them, allowing this issue to be overcome.
 */
void process_and_display_status_attribute(WendigoApp *app, char *attribute_name, char *attribute_value) {
    char *interesting_attributes[] = {"BT Classic Devices:", "BT Low Energy Devices:",
                                      "WiFi STA Devices:", "WiFi APs:"};
    uint8_t interesting_attributes_count = 4;
    uint8_t interested;
    uint16_t count = 0;
    char strVal[6];
    for (interested = 0; interested < interesting_attributes_count && strcmp(interesting_attributes[interested], attribute_name); ++interested) { }
    if (interested < interesting_attributes_count) {
        /* We're interested in this attribute */
        switch (interested) {
            // TODO: Roll these into a single loop, with case just setting some variables
            case 0:
                /* BT Classic device count - Count it ourselves for consistency */
                for (uint16_t i = 0; i < devices_count; ++i) {
                    if (devices[i]->scanType == SCAN_HCI) {
                        ++count;
                    }
                }
                snprintf(strVal, sizeof(strVal), "%d", count);
                attribute_value = strVal;
                break;
            case 1:
                /* BLE device count */
                for (uint16_t i = 0; i < devices_count; ++i) {
                    if (devices[i]->scanType == SCAN_BLE) {
                        ++count;
                    }
                }
                snprintf(strVal, sizeof(strVal), "%d", count);
                attribute_value = strVal;
                break;
            case 2:
                /* WiFi STA */
                for (uint16_t i = 0; i < devices_count; ++i) {
                    if (devices[i]->scanType == SCAN_WIFI_STA) {
                        ++count;
                    }
                }
                snprintf(strVal, sizeof(strVal), "%d", count);
                attribute_value = strVal;
                break;
            case 3:
                /* WiFi AP */
                for (uint16_t i = 0; i < devices_count; ++i) {
                    if (devices[i]->scanType == SCAN_WIFI_AP) {
                        ++count;
                    }
                }
                snprintf(strVal, sizeof(strVal), "%d", count);
                attribute_value = strVal;
                break;
        }
    }
    wendigo_scene_status_add_attribute(app, attribute_name, attribute_value);
}

/* Returns the number of bytes consumed from the buffer - DOES NOT remove consumed bytes
   from the buffer, this must be handled by the calling function.
   The received packet must follow this structure:
   * 4 bytes of 0xFF followed by 4 bytes of 0xAA
   * Attributes as specified in wendigo_packet_offsets.h
   * 4 bytes of 0xAA followed by 4 bytes of 0xFF
*/
uint16_t parseBufferBluetooth(WendigoApp *app) {
    /* Packet length is end_of_packet() + 1 because end_of_packet() points to the last byte of the packet */
    uint16_t packetLen = end_of_packet(buffer, bufferLen) + 1;
    /* Sanity check - we should have at least 55 bytes including header and footer */
    if (packetLen < (WENDIGO_OFFSET_BT_COD_LEN + PREAMBLE_LEN)) {
        /* Ignore the malformed packet if scanning has been stopped */
        if (app->is_scanning) {
            // TODO: Introducing polling to recover from an ESP32 restart can lead to corrupted packets
            //       if confirmation of scan status is interleaved with a device packet.
            //       Create a queueing mechanism in ESP32 so packets are guaranteed to be sent sequentially.
            //wendigo_display_popup(app, "Packet Error", "Bluetooth packet is shorter than expected");
        }
        // Skip this packet
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
    /* Copy fixed-byte members */
    memcpy(&(dev->radio.bluetooth.bdname_len), buffer + WENDIGO_OFFSET_BT_BDNAME_LEN, sizeof(uint8_t));
    memcpy(&(dev->radio.bluetooth.eir_len), buffer + WENDIGO_OFFSET_BT_EIR_LEN, sizeof(uint8_t));
    int32_t temp_rssi;
    memcpy(&temp_rssi, buffer + WENDIGO_OFFSET_BT_RSSI, sizeof(int32_t));
    dev->rssi = (int8_t)temp_rssi;
    memcpy(&(dev->radio.bluetooth.cod), buffer + WENDIGO_OFFSET_BT_COD, sizeof(uint32_t));
    memcpy(dev->mac, buffer + WENDIGO_OFFSET_BT_BDA, MAC_BYTES);
    memcpy(&(dev->scanType), buffer + WENDIGO_OFFSET_BT_SCANTYPE, sizeof(ScanType));
    memcpy(&(dev->tagged), buffer + WENDIGO_OFFSET_BT_TAGGED, sizeof(bool));
    memcpy(&(dev->lastSeen), buffer + WENDIGO_OFFSET_BT_LASTSEEN, sizeof(struct timeval)); // TODO: Should lastSeen be removed from here as well?
    memcpy(&(dev->radio.bluetooth.bt_services.num_services), buffer + WENDIGO_OFFSET_BT_NUM_SERVICES, sizeof(uint8_t));
    memcpy(&(dev->radio.bluetooth.bt_services.known_services_len), buffer + WENDIGO_OFFSET_BT_KNOWN_SERVICES_LEN, sizeof(uint8_t));
    uint8_t cod_len;
    memcpy(&cod_len, buffer + WENDIGO_OFFSET_BT_COD_LEN, sizeof(uint8_t));
    /* Do we have a bdname? */
    uint16_t index = WENDIGO_OFFSET_BT_BDNAME;
    /* Rudimentary buffer capacity check - Is the buffer large enough to hold the specified bdname and packet terminator? */
    if (dev->radio.bluetooth.bdname_len > 0 &&
            bufferLen > (index + dev->radio.bluetooth.bdname_len + PREAMBLE_LEN)) {
        dev->radio.bluetooth.bdname = malloc(sizeof(char) * (dev->radio.bluetooth.bdname_len + 1));
        if (dev->radio.bluetooth.bdname == NULL) {
            /* Can't store bdname so set bdname_len to 0 */
            dev->radio.bluetooth.bdname_len = 0;
        } else {
            memcpy(dev->radio.bluetooth.bdname, buffer + index, dev->radio.bluetooth.bdname_len + 1);
        }
        /* Move index past bdname and its terminating null character */
        index += (dev->radio.bluetooth.bdname_len + 1);
    }
    /* EIR? */
    if (dev->radio.bluetooth.eir_len > 0 &&
            bufferLen >= (index + dev->radio.bluetooth.eir_len + PREAMBLE_LEN)) {
        dev->radio.bluetooth.eir = malloc(sizeof(uint8_t) * dev->radio.bluetooth.eir_len);
        if (dev->radio.bluetooth.eir == NULL) {
            dev->radio.bluetooth.eir_len = 0;
        } else {
            memcpy(dev->radio.bluetooth.eir, buffer + index, dev->radio.bluetooth.eir_len);
        }
        index += dev->radio.bluetooth.eir_len;
    }
    /* Class of Device? */
    if (cod_len > 0 && bufferLen > (index + cod_len + PREAMBLE_LEN)) {
        dev->radio.bluetooth.cod_str = malloc(sizeof(char) * (cod_len + 1));
        if (dev->radio.bluetooth.cod_str != NULL) {
            memcpy(dev->radio.bluetooth.cod_str, buffer + index, cod_len + 1);
        }
        index += (cod_len + 1); /* Cater for the trailing '\0' */
    }
    // TODO: Services to go here

    /* Hopefully `index` now points to the packet terminator (unless scanning has stopped,
       then all bets are off) */
    if (app->is_scanning && memcmp(PACKET_TERM, buffer + index, PREAMBLE_LEN)) {
        // TODO: Introducing polling to recover from an ESP32 restart can lead to corrupted packets
        //       if confirmation of scan status is interleaved with a device packet.
        //       Create a queueing mechanism in ESP32 so packets are guaranteed to be sent sequentially.
        //wendigo_display_popup(app, "BT Packet Error", "Packet terminator not found where expected");
    }

    /* Add or update the device in devices[] - No longer need to check
       whether we're adding or updating, the add/update functions will call
       each other if required. */
    wendigo_add_device(app, dev);
    /* Clean up memory */
    wendigo_free_device(dev);

    /* We've consumed `index` bytes as well as the packet terminator */
    // TODO: Consider pros & cons of this vs. returning packetLen. At the very least comparing the two could help find corrupted packets.
    return index + PREAMBLE_LEN;
}

/* Returns the number of bytes consumed from the buffer - DOES NOT
   remove consumed bytes, this must be handled by the calling function. */
uint16_t parseBufferWifiAp(WendigoApp *app) {
    // TODO
    UNUSED(app);
    return 0;
}

uint16_t parseBufferWifiSta(WendigoApp *app) {
    // TODO
    UNUSED(app);
    return 0;
}

/* Returns the number of bytes consumed from the buffer - DOES NOT
   remove consumed bytes, this must be handled by the calling function. */
uint16_t parseBufferVersion(WendigoApp *app) {
    /* Find the end-of-packet sequence to determine version string length */
    uint16_t endSeq = end_of_packet(buffer, bufferLen);
    if (endSeq == bufferLen) {
        return 0; /* Packet terminator not found. This condition has already been tested for twice, don't bother taking any action */
    }
    endSeq = endSeq - PREAMBLE_LEN + 1; /* Sub 7 to reach first byte in seq */
    char *versionStr = realloc(wendigo_popup_text, sizeof(char) * endSeq);
    if (versionStr == NULL) {
        // TODO: Panic
        // For now just consume this message
        return endSeq + PREAMBLE_LEN;
    }
    wendigo_popup_text = versionStr;
    // TODO: I don't know why I implemented this as a loop. Replace loop with the following (I didn't want to make this change at the same time as other bulk changes in case of an off-by-one error)
    // memcpy(wendigo_popup_text, buffer, endSeq);
    for (int i = 0; i < endSeq; ++i) {
        wendigo_popup_text[i] = buffer[i];
    }
    wendigo_popup_text[endSeq - 1] = '\0'; /* Just in case */
    wendigo_display_popup(app, "ESP32 Version", wendigo_popup_text);
    return endSeq + PREAMBLE_LEN;
}

/** Parse a status packet and display in the status view.
 * Returns the number of bytes consumed by the function, including the end-of-packet
 * bytes. DOES NOT remove these bytes, that is handled by the calling function.
 * This function requires that Wendigo_AppViewStatus be the currently-displayed view
 * (otherwise the packet is discarded).devdevdevddevoooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;e;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;jjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjj kjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjev
 */
uint16_t parseBufferStatus(WendigoApp *app) {
    /* Ignore the packet if the status scene isn't displayed */
    uint16_t packetLen = end_of_packet(buffer, bufferLen) + 1;
    if (app->current_view != WendigoAppViewStatus) {
        return packetLen;
    }
    wendigo_scene_status_begin_layout(app);
// TODO: Document the packet somewhere. Less straightforward than the Bluetooth packet because it's a dynamic size, but it needs *something*
    uint8_t attribute_count;
    uint8_t attribute_name_len;
    uint8_t attribute_value_len;
    char *attribute_name = NULL;
    char *attribute_value = NULL;
    uint16_t offset = PREAMBLE_LEN;
    memcpy(&attribute_count, buffer + offset, sizeof(uint8_t));
    offset += sizeof(uint8_t);
    for (uint8_t i = 0; i < attribute_count; ++i) {
        /* Parse attribute name length */
        memcpy(&attribute_name_len, buffer + offset, sizeof(uint8_t));
        if (attribute_name_len == 0) {
            // TODO: Panic
            return packetLen;
        }
        offset += sizeof(uint8_t);
        /* Name */
        attribute_name = malloc(sizeof(char) * (attribute_name_len + 1));
        if (attribute_name == NULL) {
            // TODO: panic
            return packetLen;
        }
        memcpy(attribute_name, buffer + offset, attribute_name_len);
        attribute_name[attribute_name_len] = '\0';
        offset += attribute_name_len;
        /* Attribute value length */
        memcpy(&attribute_value_len, buffer + offset, sizeof(uint8_t));
        offset += sizeof(uint8_t);
        /* It's valid for this to have a length of 0 - attribute_value will be "" */
        attribute_value = malloc(sizeof(char) * (attribute_value_len + 1));
        if (attribute_value == NULL) {
            // TODO: Panic
            free(attribute_name);
            return packetLen;
        }
        memcpy(attribute_value, buffer + offset, attribute_value_len);
        attribute_value[attribute_value_len] = '\0';
        offset += attribute_value_len;
        /* Send the attribute off for validation and display */
        process_and_display_status_attribute(app, attribute_name, attribute_value);
        free(attribute_name);
        free(attribute_value);
    }
    wendigo_scene_status_finish_layout(app);

    /* buffer + offset should now point to the end of packet sequence */
    if (memcmp(PACKET_TERM, buffer + offset, PREAMBLE_LEN)) {
        // TODO: Panic
        return packetLen;
    }
    return offset + PREAMBLE_LEN;
}

/** When the end of a packet is reached this function is called to parse the
 *  buffer and take the appropriate action. We expect to see one of the following:
 *   * Begin with 0xFF,0xFF,0xFF,0xFF,0xAA,0xAA,0xAA,0xAA: Bluetooth packet
 *   * Begin with 0x99,0x99,0x99,0x99,0x11,0x11,0x11,0x11: WiFi packet
 *   * Begin with "Wendigo "                             : Version packet
 *  The end of packet is marked by 0xAA,0xAA,0xAA,0xAA,0xFF,0xFF,0xFF,0xFF.
 *  This function removes the parsed packet from the buffer, including the
 *  beginning and end of packet markers.
 */
void parseBuffer(WendigoApp *app) {
    uint16_t consumedBytes = 0;
    /* Update the last packet received time */
    app->last_packet = furi_hal_rtc_get_timestamp();
    /* We get here only after finding an end of packet sequence, so can assume
       there's a beginning of packet sequence. */
    consumedBytes = start_of_packet(buffer, bufferLen);
    /* Remove anything prior to this sequence so that the preamble begins at buffer[0] - Or is empty */
    consumeBufferBytes(consumedBytes);

    if (bufferLen > PREAMBLE_LEN && !memcmp(PREAMBLE_BT_BLE, buffer, PREAMBLE_LEN)) {
        consumedBytes = parseBufferBluetooth(app);
    } else if (bufferLen > PREAMBLE_LEN && !memcmp(PREAMBLE_WIFI_AP, buffer, PREAMBLE_LEN)) {
        consumedBytes = parseBufferWifiAp(app);
    } else if (bufferLen > PREAMBLE_LEN && !memcmp(PREAMBLE_WIFI_STA, buffer, PREAMBLE_LEN)) {
        consumedBytes = parseBufferWifiSta(app);
    } else if (bufferLen > PREAMBLE_LEN && !memcmp(PREAMBLE_STATUS, buffer, PREAMBLE_LEN)) {
        consumedBytes = parseBufferStatus(app);
    } else if (bufferLen > PREAMBLE_LEN && !memcmp(PREAMBLE_VER, buffer, PREAMBLE_LEN)) {
        consumedBytes = parseBufferVersion(app);
    } else {
        /* We reached this function by finding an end-of-packet sequence, but can't find
           a start-of-packet sequence. Assume the packet was corrupted and throw away
           everything up to and including the end-of-packet sequence */
        consumedBytes = end_of_packet(buffer, bufferLen);
        if (consumedBytes == bufferLen) {
            /* We got here by finding a packet terminator, but now can't find one. */
            // TODO: Report error
            return;
        }
        /* Currently points to the last byte in the sequence, move forward */
        ++consumedBytes;
    }
    consumeBufferBytes(consumedBytes);
}

/* Callback invoked when UART data is received. When an end-of-packet sequence is
   received the buffer is sent to the appropriate handler for display.
   At the time of writing a similar callback exists in wendigo_scene_console_output,
   that is being retained through initial stages of development because it provides
   a useful way to run ad-hoc tests from Flipper. Eventually the entire scene
   wendigo_scene_console_output will likely be removed, until then collision can
   be avoided by limiting its use, ensuring that wendigo_uart_set_handle_rx_data_cb()
   is called after using the console to re-establish this function as the UART callback.
*/
void wendigo_scan_handle_rx_data_cb(uint8_t* buf, size_t len, void* context) {
    furi_assert(context);
    WendigoApp* app = context;

    /* Extend the buffer if necessary */
    if (bufferLen + len >= bufferCap) {
        /* Extend it by the larger of len and 128 bytes do avoid constant realloc()s */
        uint16_t newCapacity = bufferCap + ((len > 128) ? len : 128);
        uint8_t *newBuffer = realloc(buffer, sizeof(uint8_t *) * newCapacity); // Behaves like malloc() when buffer==NULL
        if (newBuffer == NULL) {
            /* Out of memory */
            // TODO: Panic
            return;
        }
        buffer = newBuffer;
        bufferCap = newCapacity;
    }
    memcpy(buffer + bufferLen, buf, len);
    bufferLen += len;

    /* Parse any complete packets we have received */
    uint16_t endFound = end_of_packet(buffer, bufferLen);
    // TODO: Putting this in a loop results in an infinite loop and I can't figure out why. Fix this (YAGNI).
    if (endFound < bufferLen) {
        parseBuffer(app);
   }
}

void wendigo_free_uart_buffer() {
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
}
