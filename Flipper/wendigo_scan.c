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
uint8_t PREAMBLE_VER[]    = {'W', 'e', 'n', 'd', 'i', 'g', 'o', ' '};
uint8_t PACKET_TERM[]     = {0xAA, 0xAA, 0xAA, 0xAA, 0xFF, 0xFF, 0xFF, 0xFF};

/** Search for the start-of-packet marker in the specified byte array
 *  This function is used during parsing to skip past any extraneous bytes
 *  that are sent prior to the start-of-packet sequence.
 *  Returns `size` if not found.
 */
uint16_t start_of_packet(uint8_t *bytes, uint16_t size) {
    uint16_t result = 0;
    for (; result + PREAMBLE_LEN <= size && memcmp(bytes + result, PREAMBLE_BT_BLE, PREAMBLE_LEN) &&
            memcmp(bytes + result, PREAMBLE_WIFI, PREAMBLE_LEN) &&
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
    for (int i = 0; i < IF_COUNT; ++i) {
        /* Set command */
        cmd = (i == IF_BLE) ? 'b' : (i == IF_BT_CLASSIC) ? 'h' : 'w';
        /* arg */
        arg = (starting && app->interfaces[i].active) ? 1 : 0;
        snprintf(cmdString, CMD_LEN, "%c %d\n", cmd, arg);
        wendigo_uart_tx(app->uart, (uint8_t *)cmdString, CMD_LEN);
    }
    app->is_scanning = starting;
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

/* Adds the specified device to bt_devices[], extending the length of bt_devices[] if necessary.
   DOES NOT check to ensure the device is not already present in bt_devices[].
   Return true if the device was successfully added to bt_devices[].
   NOTE: The provided flipper_bt_device object is retained in bt_devices[] - It MUST NOT be freed by the caller */
bool wendigo_add_bt_device(flipper_bt_device *dev) {
    /* Increase capacity of bt_devices[] by an additional 10 if necessary */
    if (bt_devices == NULL || bt_devices_capacity == bt_devices_count) {
        flipper_bt_device **new_devices = realloc(bt_devices, sizeof(flipper_bt_device *) * (bt_devices_capacity + 10));
        if (new_devices == NULL) {
            /* Can't store the device */
            return false;
        }
        bt_devices = new_devices;
        bt_devices_capacity += 10;
    }
    bt_devices[bt_devices_count++] = dev;
    return true;
}

/* Locates a device in bt_devices[] with the same BDA as `dev` and updates the object's fields to match `dev`.
   If the specified device does not exist in bt_devices[], or another error occurs, the function returns false,
   otherwise true is returned to indicate success. */
bool wendigo_update_bt_device(flipper_bt_device *dev) {
    uint16_t idx = bt_device_index(dev);
    if (idx == bt_devices_count) {
        /* Device doesn't exist in bt_devices[] */
        return false;
    }
    /* Update bt_devices[idx] */
    if (dev->cod_str != NULL) {
        if (bt_devices[idx]->cod_str != NULL) {
            free(bt_devices[idx]->cod_str);
        }
        bt_devices[idx]->cod_str = dev->cod_str;
    }
    if (dev->dev.bdname_len > 0 && dev->dev.bdname != NULL) {
        if (bt_devices[idx]->dev.bdname_len > 0 && bt_devices[idx]->dev.bdname != NULL) {
            free(bt_devices[idx]->dev.bdname);
        }
        bt_devices[idx]->dev.bdname_len = dev->dev.bdname_len;
        bt_devices[idx]->dev.bdname = dev->dev.bdname;
    }
    if (dev->dev.cod != 0) {
        bt_devices[idx]->dev.cod = dev->dev.cod;
    }
    if (dev->dev.eir_len > 0 && dev->dev.eir != NULL) {
        if (bt_devices[idx]->dev.eir_len > 0 && bt_devices[idx]->dev.eir != NULL) {
            free(bt_devices[idx]->dev.eir);
        }
        bt_devices[idx]->dev.eir_len = dev->dev.eir_len;
        bt_devices[idx]->dev.eir = dev->dev.eir;
    }
    bt_devices[idx]->dev.lastSeen = dev->dev.lastSeen;
    bt_devices[idx]->dev.rssi = dev->dev.rssi;
    bt_devices[idx]->dev.scanType = dev->dev.scanType;
    bt_devices[idx]->dev.tagged = dev->dev.tagged;
    /* Now update service descriptors */
    if (dev->dev.bt_services.num_services > 0 && dev->dev.bt_services.num_services != bt_devices[idx]->dev.bt_services.num_services) {
        if (bt_devices[idx]->dev.bt_services.num_services > 0 && bt_devices[idx]->dev.bt_services.service_uuids != NULL) {
            free(bt_devices[idx]->dev.bt_services.service_uuids);
        }
        bt_devices[idx]->dev.bt_services.num_services = dev->dev.bt_services.num_services;
        bt_devices[idx]->dev.bt_services.service_uuids = dev->dev.bt_services.service_uuids;
    }
    if (dev->dev.bt_services.known_services_len > 0 && dev->dev.bt_services.known_services_len != bt_devices[idx]->dev.bt_services.known_services_len) {
        if (bt_devices[idx]->dev.bt_services.known_services_len > 0 && bt_devices[idx]->dev.bt_services.known_services != NULL) {
            free(bt_devices[idx]->dev.bt_services.known_services);
        }
        bt_devices[idx]->dev.bt_services.known_services_len = dev->dev.bt_services.known_services_len;
        bt_devices[idx]->dev.bt_services.known_services = dev->dev.bt_services.known_services;
    }
    return true;
}

void wendigo_free_bt_devices() {
    flipper_bt_device *dev;
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
            dev = bt_devices[i];
            if (dev->cod_str != NULL) {
                free(dev->cod_str);
            }
            if (dev->dev.bdname_len > 0 && dev->dev.bdname != NULL) {
                free(dev->dev.bdname);
                dev->dev.bdname = NULL;
                dev->dev.bdname_len = 0;
            }
            if (dev->dev.eir_len > 0 && dev->dev.eir != NULL) {
                free(dev->dev.eir);
                dev->dev.eir = NULL;
                dev->dev.eir_len = 0;
            }
            if (dev->dev.bt_services.known_services_len > 0 && dev->dev.bt_services.known_services != NULL) {
                // TODO: This is a ** - Loop through it's contents when services are implemented
                free(dev->dev.bt_services.known_services);
                dev->dev.bt_services.known_services = NULL;
                dev->dev.bt_services.known_services_len = 0;
            }
            if (dev->dev.bt_services.num_services > 0 && dev->dev.bt_services.service_uuids != NULL) {
                free(dev->dev.bt_services.service_uuids);
                dev->dev.bt_services.service_uuids = NULL;
                dev->dev.bt_services.num_services = 0;
            }
            dev = NULL;
            free(bt_devices[i]);
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
        // TODO: I'm not sure what to do in this case
        wendigo_display_popup(app, "Packet Error", "Bluetooth packet is shorter than expected");
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
    // TODO: Need to check bufferLen from here on
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

    /* Hopefully `index` now points to the packet terminator */
    if (memcmp(PACKET_TERM, buffer + index, PREAMBLE_LEN)) {
        // TODO: Panic & recover
    }

/*
        // TODO Remove this debugging stuff
        if (dev->dev.bdname_len > 0 && dev->dev.bdname != NULL) {
            // Found \"%s\"
            char *nameStr = malloc(sizeof(char) * (9 + strlen(dev->dev.bdname)));
            snprintf(nameStr, 9 + strlen(dev->dev.bdname), "Found \"%s\"", dev->dev.bdname);
            wendigo_display_popup(app, "parseBluetooth()", nameStr);
        }
*/
    /* Does this device already exist in bt_devices[]? */
    if (bt_device_exists(dev)) {
        wendigo_update_bt_device(dev);
    } else {
        wendigo_add_bt_device(dev);
    }
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
