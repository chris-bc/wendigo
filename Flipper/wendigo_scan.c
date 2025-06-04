#include "wendigo_scan.h"
#include "wendigo_packet_offsets.h"

uint8_t *buffer = NULL;
uint16_t bufferLen = 0; // 65535 should be plenty of length
uint16_t bufferCap = 0; // Buffer capacity - I don't want to allocate 65kb, but don't want to constantly realloc
char *wendigo_popup_text = NULL; // I suspect the popup text is going out of scope when declared at function scope

/* Device caches */
flipper_bt_device **bt_devices = NULL;
flipper_bt_device **bt_selected_devices = NULL;
uint16_t bt_devices_count = 0;
uint16_t bt_selected_devices_count = 0;
uint16_t bt_devices_capacity = 0;
uint16_t bt_selected_devices_capacity = 0;
// TODO: WiFi

/* Packet identifiers */
uint8_t PREAMBLE_LEN = 8;
uint8_t PREAMBLE_BT_BLE[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xAA, 0xAA, 0xAA, 0xAA};
uint8_t PREAMBLE_WIFI[]   = {0x99, 0x99, 0x99, 0x99, 0x11, 0x11, 0x11, 0x11};
uint8_t PREAMBLE_STATUS[] = {0xEE, 0xEE, 0xEE, 0xEE, 0xBB, 0xBB, 0xBB, 0xBB};
uint8_t PREAMBLE_VER[]    = {'W', 'e', 'n', 'd', 'i', 'g', 'o', ' '};
uint8_t PACKET_TERM[]     = {0xAA, 0xAA, 0xAA, 0xAA, 0xFF, 0xFF, 0xFF, 0xFF};

/* How much will we increase bt_devices[] by when additional space is needed? */
#define INC_BT_DEVICE_CAPACITY_BY   10

/** Search for the start-of-packet marker in the specified byte array
 *  This function is used during parsing to skip past any extraneous bytes
 *  that are sent prior to the start-of-packet sequence.
 *  Returns `size` if not found.
 */
uint16_t start_of_packet(uint8_t *bytes, uint16_t size) {
    uint16_t result = 0;
    for (; result + PREAMBLE_LEN <= size && memcmp(bytes + result, PREAMBLE_BT_BLE, PREAMBLE_LEN) &&
            memcmp(bytes + result, PREAMBLE_WIFI, PREAMBLE_LEN) &&
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
    for (; result < size && memcmp(theBytes + result - PREAMBLE_LEN + 1, PACKET_TERM, PREAMBLE_LEN); ++result) { }
    return result;
}

/** Consume `bytes` bytes from `buffer`.
 *  This function does not alter buffer capacity, but updates buffer and bufferLen
 *  to remove the specified number of bytes from the beginning of the buffer.
 */
void consumeBufferBytes(uint16_t bytes) {
    if (bytes == 0) {
        // That was a bit of a waste
        return;
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
   Returns size if not found (which is outside the array bounds). */
uint16_t bytes_contains_newline(uint8_t *theBytes, uint16_t size) {
    uint16_t result = 0;
    for (; result < size && theBytes[result] != '\n'; ++result) { }
    return result;
}

/* Returns the index of the first occurrence of '\n' in `theString`, or strlen(theString) if not found.
   This function requires `theString` to be null terminated. */
uint16_t string_contains_newline(char *theString) {
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
bool wendigo_set_bt_device_selected(flipper_bt_device *device, bool selected) {
    if (selected && !device->dev.tagged) {
        /* Add device to bt_selected_devices[] */
        if (bt_selected_devices_capacity == bt_selected_devices_count) {
            /* Extend bt_selected_devices[] */
            flipper_bt_device **new_devices = realloc(bt_selected_devices, sizeof(flipper_bt_device *) * (bt_selected_devices_capacity + 1));
            if (new_devices == NULL) {
                // TODO: Panic - Display an error popup or something
                return false;
            }
            bt_selected_devices = new_devices;
            ++bt_selected_devices_capacity;
        }
        bt_selected_devices[bt_selected_devices_count++] = device;
        // TODO: Sort
    } else if (device->dev.tagged && !selected) {
        /* Remove device from bt_selected_devices[] */
        /* Not using 16 bits because nobody's going to select 255 devices */
        uint8_t index = (uint8_t)bt_custom_device_index(device, bt_selected_devices, bt_selected_devices_count);
        if (index < bt_selected_devices_count) {
            /* Shuffle forward all subsequent devices, replacing device at `index` */
            for(++index; index < bt_selected_devices_count; ++index) {
                bt_selected_devices[index - 1] = bt_selected_devices[index];
            }
            bt_selected_devices[bt_selected_devices_count--] = NULL;
        }
    }
    device->dev.tagged = selected;
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

/* Returns the index into `array` of the device with BDA matching dev->dev.bda.
   Returns `array_count` if the device was not found. */
uint16_t bt_custom_device_index(flipper_bt_device *dev, flipper_bt_device **array, uint16_t array_count) {
    uint16_t result;
    for (result = 0; result < array_count && memcmp(dev->dev.bda, array[result]->dev.bda, MAC_BYTES); ++result) { }
    return result;
}

/* Returns the index into bt_devices[] of the device with BDA matching dev->dev.bda.
   Returns bt_devices_count if the device was not found. */
uint16_t bt_device_index(flipper_bt_device *dev) {
    return bt_custom_device_index(dev, bt_devices, bt_devices_count);
}

/* Determines whether a device with the BDA dev->bda exists in bt_devices[] */
bool bt_device_exists(flipper_bt_device *dev) {
    return bt_device_index(dev) < bt_devices_count;
}

/** Add the specified device to bt_devices[], extending the length of bt_devices[] if necessary.
 * If the specified device has a BDA which is already present in bt_devices[] a new entry will
 * not be made, instead the element with the same BDA will be updated based on the specified device.
 * Returns true if the device was successfully added to (or updated in) bt_devices[].
 * NOTE: The calling function may free the specified flipper_bt_device or any of its attributes
 * when this function returns. To minimise the likelihood of memory leaks this function will allocate
 * its own memory to hold the specified device and its attributes.
 */
bool wendigo_add_bt_device(WendigoApp *app, flipper_bt_device *dev) {
    uint16_t idx = bt_device_index(dev);
    if (idx < bt_devices_count) {
        /* A device with the provided BDA already exists - Update that instead */
        return wendigo_update_bt_device(app, dev);
    }
    /* Adding to bt_devices - Increase capacity by an additional INC_BT_DEVICE_CAPACITY_BY if necessary */
    if (bt_devices == NULL || bt_devices_capacity == bt_devices_count) {
        flipper_bt_device **new_devices = realloc(bt_devices, sizeof(flipper_bt_device *) * (bt_devices_capacity + INC_BT_DEVICE_CAPACITY_BY));
        if (new_devices == NULL) {
            /* Can't store the device */
            return false;
        }
        bt_devices = new_devices;
        bt_devices_capacity += INC_BT_DEVICE_CAPACITY_BY;
    }
    flipper_bt_device *new_device = malloc(sizeof(flipper_bt_device));
    if (new_device == NULL) {
        /* That's unfortunate */
        return false;
    }
    new_device->dev.rssi = dev->dev.rssi;
    new_device->dev.cod = dev->dev.cod;
    new_device->dev.scanType = dev->dev.scanType;
    /* Don't copy tagged status - A new device is not tagged. */
    new_device->dev.tagged = false;
    /* ESP32 doesn't know the real time/date so overwrite the lastSeen value
       Casting directly to time_t is actually a long long int, so that works OK */
    new_device->dev.lastSeen.tv_sec = (time_t)furi_hal_rtc_get_timestamp();
    /* Copy BDA */
    memcpy(new_device->dev.bda, dev->dev.bda, MAC_BYTES);
    new_device->view = dev->view; /* Copy the view pointer if we have one */
    if (dev->cod_str != NULL) {
        new_device->cod_str = malloc(sizeof(char) * (strlen(dev->cod_str) + 1));
        if (new_device->cod_str != NULL) {
            /* No need to handle allocation failure - we simply don't copy cod_str */
            strncpy(new_device->cod_str, dev->cod_str, strlen(dev->cod_str));
        }
    } else {
        new_device->cod_str = NULL;
    }
    /* Device name */
    new_device->dev.bdname_len = dev->dev.bdname_len;
    if (dev->dev.bdname_len > 0 && dev->dev.bdname != NULL) {
        new_device->dev.bdname = malloc(sizeof(char) * (dev->dev.bdname_len + 1));
        if (new_device->dev.bdname == NULL) {
            /* Skip the device's bdname and hope we have memory for it next time it's seen */
            new_device->dev.bdname_len = 0;
        } else {
            strncpy(new_device->dev.bdname, dev->dev.bdname, dev->dev.bdname_len);
        }
    } else {
        new_device->dev.bdname = NULL;
        new_device->dev.bdname_len = 0;
    }
    /* EIR */
    new_device->dev.eir_len = dev->dev.eir_len;
    if (dev->dev.eir_len > 0 && dev->dev.eir != NULL) {
        new_device->dev.eir = malloc(sizeof(uint8_t) * dev->dev.eir_len);
        if (new_device->dev.eir == NULL) {
            /* Skip EIR from this packet */
            new_device->dev.eir_len = 0;
        } else {
            memcpy(new_device->dev.eir, dev->dev.eir, dev->dev.eir_len);
        }
    } else {
        new_device->dev.eir = NULL;
        new_device->dev.eir_len = 0;
    }
    /* BT services */
    new_device->dev.bt_services.num_services = dev->dev.bt_services.num_services;
    if (dev->dev.bt_services.num_services > 0 && dev->dev.bt_services.service_uuids != NULL) {
        new_device->dev.bt_services.service_uuids = malloc(sizeof(void *) * dev->dev.bt_services.num_services);
        if (new_device->dev.bt_services.service_uuids == NULL) {
            new_device->dev.bt_services.num_services = 0;
        } else {
            memcpy(new_device->dev.bt_services.service_uuids, dev->dev.bt_services.service_uuids,
                sizeof(void *) * dev->dev.bt_services.num_services);
        }
    } else {
        new_device->dev.bt_services.service_uuids = NULL;
        new_device->dev.bt_services.num_services = 0;
    }
    new_device->dev.bt_services.known_services_len = dev->dev.bt_services.known_services_len;
    if (dev->dev.bt_services.known_services_len > 0 && dev->dev.bt_services.known_services != NULL) {
        /* known_services is an array of pointers - this is where we stop controlling memory allocation 
           so that bt_uuid's can be allocated a single time and reused. */
        new_device->dev.bt_services.known_services = malloc(sizeof(bt_uuid *) * dev->dev.bt_services.known_services_len);
        if (new_device->dev.bt_services.known_services == NULL) {
            new_device->dev.bt_services.known_services_len = 0;
        } else {
            memcpy(new_device->dev.bt_services.known_services, dev->dev.bt_services.known_services,
                sizeof(bt_uuid *) * dev->dev.bt_services.known_services_len);
        }
    } else {
        new_device->dev.bt_services.known_services = NULL;
        new_device->dev.bt_services.known_services_len = 0;
    }

    bt_devices[bt_devices_count++] = new_device;

    /* If the device list scene is currently displayed, add the device to the UI */
    if (app->current_view == WendigoAppViewDeviceList) {
        wendigo_scene_device_list_update(app, new_device);
    }

    return true;
}

/* Locates a device in bt_devices[] with the same BDA as `dev` and updates the object's fields to match `dev`.
   If the specified device does not exist in bt_devices[], or another error occurs, the function returns false,
   otherwise true is returned to indicate success. */

/** Locate the device in bt_devices[] with the same BDA as `dev` and update the object based
 * on the contents of `dev`. If a device with the specified BDA does not exist a new device will
 * be added to bt_devices[].
 * Returns true if the specified device has been updated (or a new device added) successfully.
 * NOTE: The calling function may free `dev` or any of its members immediately upon this function
 * returning. To minimise the likelihood of memory leaks, the device cache management functions
 * manage the allocation and free'ing of all components of the device cache *except for* bt_uuid
 * objects that form part of service descriptors.
 */
bool wendigo_update_bt_device(WendigoApp *app, flipper_bt_device *dev) {
    uint16_t idx = bt_device_index(dev);
    if (idx == bt_devices_count) {
        /* Device doesn't exist in bt_devices[] */
        return wendigo_add_bt_device(app, dev);
    }

    flipper_bt_device *target = bt_devices[idx];
    /* Copy standard attributes */
    target->dev.rssi = dev->dev.rssi;
    target->dev.cod = dev->dev.cod;
    target->dev.scanType = dev->dev.scanType;
    /* Replace lastSeen - cast to long long int (aka time_t) */
    target->dev.lastSeen.tv_sec = (time_t)furi_hal_rtc_get_timestamp();
    /* cod_str present in update? */
    if (dev->cod_str != NULL && strlen(dev->cod_str) > 0) {
        char *new_cod = realloc(target->cod_str, sizeof(char) * (strlen(dev->cod_str) + 1));
        if (new_cod != NULL) {
            strncpy(new_cod, dev->cod_str, strlen(dev->cod_str));
            target->cod_str = new_cod;
        }
    }
    /* Is bdname in update? */
    if (dev->dev.bdname_len > 0 && dev->dev.bdname != NULL) {
        char *new_name = realloc(target->dev.bdname, sizeof(char) * (dev->dev.bdname_len + 1));
        if (new_name != NULL) {
            strncpy(new_name, dev->dev.bdname, dev->dev.bdname_len);
            target->dev.bdname = new_name;
            target->dev.bdname_len = dev->dev.bdname_len;
        }
    }
    /* How about EIR? */
    if (dev->dev.eir_len > 0 && dev->dev.eir != NULL) {
        uint8_t *new_eir = realloc(target->dev.eir, sizeof(uint8_t) * dev->dev.eir_len);
        if (new_eir != NULL) {
            memcpy(new_eir, dev->dev.eir, sizeof(uint8_t) * dev->dev.eir_len);
            target->dev.eir = new_eir;
            target->dev.eir_len = dev->dev.eir_len;
        }
    }
    /* Number of services */
    if (dev->dev.bt_services.num_services > 0 && dev->dev.bt_services.service_uuids != NULL) {
        void *new_svcs = realloc(target->dev.bt_services.service_uuids, sizeof(void *) * dev->dev.bt_services.num_services);
        if (new_svcs != NULL) {
            memcpy(new_svcs, dev->dev.bt_services.service_uuids, sizeof(void *) * dev->dev.bt_services.num_services);
            target->dev.bt_services.service_uuids = new_svcs;
            target->dev.bt_services.num_services = dev->dev.bt_services.num_services;
        }
    }
    /* Known services */
    if (dev->dev.bt_services.known_services_len > 0 && dev->dev.bt_services.known_services != NULL) {
        bt_uuid **new_known_svcs = realloc(target->dev.bt_services.known_services, sizeof(bt_uuid *) * dev->dev.bt_services.known_services_len);
        if (new_known_svcs != NULL) {
            memcpy(new_known_svcs, dev->dev.bt_services.known_services, sizeof(bt_uuid *) * dev->dev.bt_services.known_services_len);
            target->dev.bt_services.known_services = new_known_svcs;
            target->dev.bt_services.known_services_len = dev->dev.bt_services.known_services_len;
        }
    }

    /* Update the device list if it's currently displayed */
    if (app->current_view == WendigoAppViewDeviceList) {
        wendigo_scene_device_list_update(app, bt_devices[idx]);
    } else if (app->current_view == WendigoAppViewDeviceDetail) { // && selectedDevice == bt_devices[idx]
        // TODO: Update existing view
    }

    return true;
}

/** Free all memory allocated to the specified device.
 * After free'ing its members this function will also free `dev` itself.
 * dev->view will not be touched by this function because it's a scary UI-layer thing.
 */
void wendigo_free_bt_device(flipper_bt_device *dev) {
    if (dev == NULL) {
        return;
    }
    if (dev->cod_str != NULL) {
        free(dev->cod_str);
    }
    if (dev->dev.bdname != NULL) {
        free(dev->dev.bdname);
    }
    if (dev->dev.eir != NULL) {
        free(dev->dev.eir);
    }
    if (dev->dev.bt_services.service_uuids != NULL) {
        free(dev->dev.bt_services.service_uuids);
    }
    if (dev->dev.bt_services.known_services != NULL) {
        // TODO: After implementing BT services, ensure that the bt_uuid's are free'd somewhere
        free(dev->dev.bt_services.known_services);
    }
    free(dev);
}

void wendigo_free_bt_devices() {
    /* Start by freeing bt_selected_devices[] */
    if (bt_selected_devices_capacity > 0 && bt_selected_devices != NULL) {
        free(bt_selected_devices);
        bt_selected_devices = NULL;
        bt_selected_devices_count = 0;
        bt_selected_devices_capacity = 0;
    }
    /* For bt_devices[] we also want to free device attributes and the device itself */
    if (bt_devices_capacity > 0 && bt_devices != NULL) {
        for (int i = 0; i < bt_devices_count; ++i) {
            wendigo_free_bt_device(bt_devices[i]);
        }
        free(bt_devices);
        bt_devices = NULL;
        bt_devices_count = 0;
        bt_devices_capacity = 0;
    }
}

/* Returns the number of bytes consumed from the buffer - DOES NOT
   remove consumed bytes from the buffer, this must be handled by
   the calling function.
   The received packet must follow this structure:
   * 4 bytes of 0xFF followed by 4 bytes of 0xAA
   * Attributes as specified in wendigo_packet_offsets.h
   * 4 bytes of 0xAA followed by 4 bytes of 0xFF
*/
uint16_t parseBufferBluetooth(WendigoApp *app) {
    /* Sanity check - we should have at least 55 bytes including header and footer */
    uint16_t packetLen = end_of_packet(buffer, bufferLen);
    if (packetLen < (WENDIGO_OFFSET_BT_COD_LEN + PREAMBLE_LEN)) {
        /* If scanning has been stopped quietly drop this packet */
        if (app->is_scanning) {
            // TODO: I'm not sure what to do in this case
            wendigo_display_popup(app, "Packet Error", "Bluetooth packet is shorter than expected");
        }
        // Skip this packet?
        return packetLen;
    }
    flipper_bt_device *dev = malloc(sizeof(flipper_bt_device));
    if (dev == NULL) {
        // TODO: Panic. For now just skip the device
        return packetLen;
    }
    dev->dev.bdname = NULL;
    dev->dev.eir = NULL;
    dev->dev.bt_services.known_services = NULL;
    dev->dev.bt_services.service_uuids = NULL;
    dev->cod_str = NULL;
    dev->view = NULL;
    /* Copy fixed-byte members */
    memcpy(&(dev->dev.bdname_len), buffer + WENDIGO_OFFSET_BT_BDNAME_LEN, sizeof(uint8_t));
    memcpy(&(dev->dev.eir_len), buffer + WENDIGO_OFFSET_BT_EIR_LEN, sizeof(uint8_t));
    memcpy(&(dev->dev.rssi), buffer + WENDIGO_OFFSET_BT_RSSI, sizeof(int32_t));
    memcpy(&(dev->dev.cod), buffer + WENDIGO_OFFSET_BT_COD, sizeof(uint32_t));
    memcpy(dev->dev.bda, buffer + WENDIGO_OFFSET_BT_BDA, MAC_BYTES);
    memcpy(&(dev->dev.scanType), buffer + WENDIGO_OFFSET_BT_SCANTYPE, sizeof(ScanType));
    memcpy(&(dev->dev.tagged), buffer + WENDIGO_OFFSET_BT_TAGGED, sizeof(bool));
    memcpy(&(dev->dev.lastSeen), buffer + WENDIGO_OFFSET_BT_LASTSEEN, sizeof(struct timeval));
    memcpy(&(dev->dev.bt_services.num_services), buffer + WENDIGO_OFFSET_BT_NUM_SERVICES, sizeof(uint8_t));
    memcpy(&(dev->dev.bt_services.known_services_len), buffer + WENDIGO_OFFSET_BT_KNOWN_SERVICES_LEN, sizeof(uint8_t));
    uint8_t cod_len;
    memcpy(&cod_len, buffer + WENDIGO_OFFSET_BT_COD_LEN, sizeof(uint8_t));
    /* Do we have a bdname? */
    uint16_t index = WENDIGO_OFFSET_BT_BDNAME;
    if (dev->dev.bdname_len > 0 && bufferLen > index + dev->dev.bdname_len) {
        dev->dev.bdname = malloc(sizeof(char) * (dev->dev.bdname_len + 1));
        if (dev->dev.bdname != NULL) {
            memcpy(dev->dev.bdname, buffer + index, dev->dev.bdname_len + 1);
        }
        index += (dev->dev.bdname_len + 1);
    }
    /* EIR? */
    if (dev->dev.eir_len > 0 && bufferLen >= index + dev->dev.eir_len) {
        dev->dev.eir = malloc(sizeof(char) * dev->dev.eir_len);
        if (dev->dev.eir != NULL) {
            memcpy(dev->dev.eir, buffer + index, dev->dev.eir_len);
        }
        index += dev->dev.eir_len;
    }
    /* Class of Device? */
    if (cod_len > 0 && bufferLen > index + cod_len) {
        dev->cod_str = malloc(sizeof(char) * (cod_len + 1));
        if (dev->cod_str != NULL) {
            memcpy(dev->cod_str, buffer + index, cod_len + 1);
        }
        index += (cod_len + 1);
    }
    // TODO: Services to go here

    /* Hopefully `index` now points to the packet terminator (unless scanning has stopped,
       then all bets are off) */
    if (app->is_scanning && memcmp(PACKET_TERM, buffer + index, PREAMBLE_LEN)) {
        // TODO: Panic & recover
        wendigo_display_popup(app, "BT Packet Error", "Packet terminator not found where expected");
    }

    /* Add or update the device in bt_devices[] - No longer need to check
       whether we're adding or updating, the add/update functions will call
       each other if required. */
    wendigo_add_bt_device(app, dev);
    /* Clean up memory */
    wendigo_free_bt_device(dev);

    /* We've consumed `index` bytes as well as the packet terminator */
    return index + PREAMBLE_LEN - 1;
}

/* Returns the number of bytes consumed from the buffer - DOES NOT
   remove consumed bytes, this must be handled by the calling function. */
uint16_t parseBufferWifi(WendigoApp *app) {
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
        return 0; /* Already been tested twice, don't bother alerting */
    }
    endSeq = endSeq - PREAMBLE_LEN + 1; /* Sub 7 to reach first byte in seq */
    char *versionStr = realloc(wendigo_popup_text, sizeof(char) * endSeq);
    if (versionStr == NULL) {
        // TODO: Panic
        // For now just consume this message
        endSeq = end_of_packet(buffer, bufferLen);
        if (endSeq < bufferLen) {
            ++endSeq;
        }
        return endSeq;
    }
    wendigo_popup_text = versionStr;
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
 * This function requires that Wendigo_AppViewStatus be the currently-displayed view.
 */
uint16_t parseBufferStatus(WendigoApp *app) {
    /* Ignore the packet if the status scene isn't displayed */
    if (app->current_view != WendigoAppViewStatus) {
        return end_of_packet(buffer, bufferLen);
    }
    wendigo_scene_status_begin_layout(app);

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
            return end_of_packet(buffer, bufferLen);
        }
        offset += sizeof(uint8_t);
        /* Name */
        attribute_name = malloc(sizeof(char) * (attribute_name_len + 1));
        if (attribute_name == NULL) {
            // TODO: panic
            return end_of_packet(buffer, bufferLen);
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
            return end_of_packet(buffer, bufferLen);
        }
        memcpy(attribute_value, buffer + offset, attribute_value_len);
        attribute_value[attribute_value_len] = '\0';
        offset += attribute_value_len;
        /* Add the attribute to the var_item_list */
        wendigo_scene_status_add_attribute(app, attribute_name, attribute_value);
        free(attribute_name);
        free(attribute_value);
    }
    wendigo_scene_status_finish_layout(app);

    /* buffer + offset should now point to the end of packet sequence */
    if (memcmp(PACKET_TERM, buffer + offset, PREAMBLE_LEN)) {
        // TODO: Panic
        return end_of_packet(buffer, bufferLen);
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
    /* We get here only after finding an end of packet sequence, so can assume
       there's a beginning of packet sequence. */
    consumedBytes = start_of_packet(buffer, bufferLen);
    /* Remove them now so that the preamble begins at buffer[0] - Or is empty */
    consumeBufferBytes(consumedBytes);

    if (bufferLen >= PREAMBLE_LEN && !memcmp(PREAMBLE_BT_BLE, buffer, PREAMBLE_LEN)) {
        consumedBytes = parseBufferBluetooth(app);
    } else if (bufferLen >= PREAMBLE_LEN && !memcmp(PREAMBLE_WIFI, buffer, PREAMBLE_LEN)) {
        consumedBytes = parseBufferWifi(app);
    } else if (bufferLen >= PREAMBLE_LEN && !memcmp(PREAMBLE_STATUS, buffer, PREAMBLE_LEN)) {
        consumedBytes = parseBufferStatus(app);
    } else if (bufferLen >= PREAMBLE_LEN && !memcmp(PREAMBLE_VER, buffer, PREAMBLE_LEN)) {
        consumedBytes = parseBufferVersion(app);
    } else {
        /* We reached this function by finding an end-of-packet sequence, but can't find
           a start-of-packet sequence. Throw away everything up to the end-of-packet seq. */
        consumedBytes = end_of_packet(buffer, bufferLen);
        if (consumedBytes == bufferLen) {
            // Hmmmm
            return;
        }
        /* Currently points to the last byte in the sequence, move forward */
        ++consumedBytes;
    }
    consumeBufferBytes(consumedBytes);
}

/* Callback invoked when UART data is received. When an end-of-message packet is
   received it is sent to the appropriate handler for display.
    (NOTE: At the time of writing the very fancy-sounding "end-of-message packet"
           is a newline ('\n'))
   At the time of writing a similar callback exists in wendigo_scene_console_output,
   that is being retained through initial stages of development because it provides
   a useful way to run ad-hoc tests from Flipper, with "List Devices" currently
   providing the ability to send arbitrary UART commands and display results to the
   console. Eventually the entire scene wendigo_scene_console_output will likely be
   removed, until then collision can be avoided by limiting its use, ensuring that
   wendigo_uart_set_handle_rx_data_cb() is called after using the console to
   re-establish this function as the UART callback.
*/
void wendigo_scan_handle_rx_data_cb(uint8_t* buf, size_t len, void* context) {
    furi_assert(context);
    WendigoApp* app = context;

    /* Extend the buffer if necessary */
    if (bufferLen + len >= bufferCap) {
        /* Extend it by the larger of len and 128 - wendigo_bt_device is 72 bytes + name + EIR + preamble */
        uint16_t newCapacity = bufferCap + ((len > 128) ? len : 128);
        uint8_t *newBuffer = realloc(buffer, newCapacity); // Behaves like malloc() when buffer==NULL
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

    /* Parse any complete transmissions we have received */
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
    if (wendigo_popup_text != NULL) {
        free(wendigo_popup_text);
        wendigo_popup_text = NULL;
    }
}
