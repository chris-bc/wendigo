#include "wendigo_scan.h"

/* Device caches */
flipper_bt_device **bt_devices = NULL;
uint16_t bt_devices_count = 0;
uint16_t bt_devices_capacity = 0;
// TODO: WiFi

uint8_t *buffer = NULL;
uint16_t bufferLen = 0; // 65535 should be plenty of length
uint16_t bufferCap = 0; // Buffer capacity - I don't want to allocate 65kb, but don't want to constantly realloc
char *wendigo_popup_text = NULL; // I suspect the popup text is going out of scope when declared at function scope

/* Packet identifiers */
uint8_t PREAMBLE_LEN = 8;
uint8_t PREAMBLE_BT_BLE[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xAA, 0xAA, 0xAA, 0xAA};
uint8_t PREAMBLE_WIFI[]   = {0x99, 0x99, 0x99, 0x99, 0x11, 0x11, 0x11, 0x11};
uint8_t PREAMBLE_VER[]    = {'W', 'e', 'n', 'd', 'i', 'g', 'o', ' '};

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

/* Returns the index into bt_devices[] of the device with BDA matching dev->bda.
   Returns bt_devices_count if the device was not found. */
uint16_t bt_device_index(flipper_bt_device *dev) {
    uint16_t result;
    for (result = 0; result < bt_devices_count && memcmp(dev->dev.bda, bt_devices[result]->dev.bda, BDA_LEN); ++result) { }
    return result;
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

/* Returns the number of bytes consumed from the buffer - DOES NOT
   remove consumed bytes from the buffer, this must be handled by
   the calling function.
   The received packet must follow this structure:
   * 4 bytes of 0xFF followed by 4 bytes of 0xAA
   * Device structure (sizeof(wendigo_bt_device))
   * bdname if present (wendigo_bt_device.bdname_len + 1 bytes)
   * eir if present (wendigo_bt_device.eir_len bytes)
   * strlen(cod_short) (1 byte)
   * cod_short (strlen(cod_short) + 1 bytes)
   * Newline ('\n')
*/
uint16_t parseBufferBluetooth(WendigoApp *app) {
    /* Skip the preamble */
    uint16_t index = PREAMBLE_LEN;
    /* Ensure we have enough bytes for a wendigo_bt_device */
    if (bufferLen >= index + sizeof(wendigo_bt_device)) {
        flipper_bt_device *dev = malloc(sizeof(flipper_bt_device));
        if (dev == NULL) {
            // TODO: Panic. For now just skip the device
            return bytes_contains_newline(buffer, bufferLen);
        }
        /* Copy bytes into `dev->dev` */
        memcpy(&(dev->dev), buffer + index, sizeof(wendigo_bt_device));
        index += sizeof(wendigo_bt_device);
        dev->dev.bdname = NULL;
        dev->dev.bt_services.known_services = NULL;
        dev->dev.bt_services.service_uuids = NULL;
        dev->dev.bt_services.known_services_len = 0;
        dev->dev.bt_services.num_services = 0;
        dev->dev.eir = NULL;
        dev->dev.cod = 0;
        dev->cod_str = NULL;
        /* Do we have a bdname? */
        if (dev->dev.bdname_len > 0 && bufferLen >= (index + dev->dev.bdname_len + 1)) {
            dev->dev.bdname = malloc(sizeof(char) * (dev->dev.bdname_len + 1));
            if (dev->dev.bdname == NULL) {
                // TODO: Panic. For now just skip the name
                index += (dev->dev.bdname_len + 1);
                dev->dev.bdname_len = 0;
            } else {
                memcpy(dev->dev.bdname, buffer + index, dev->dev.bdname_len + 1); // Include NULL terminator
                index += (dev->dev.bdname_len + 1);
            }
        }
        /* Do we have EIR? */
        if (dev->dev.eir_len > 0 && bufferLen >= index + dev->dev.eir_len) {
            dev->dev.eir = malloc(dev->dev.eir_len);
            if (dev->dev.eir == NULL) {
                // TODO: Panic. For now just skip EIR
                index += dev->dev.eir_len;
                dev->dev.eir_len = 0;
            } else {
                memcpy(dev->dev.eir, buffer + index, dev->dev.eir_len);
                index += dev->dev.eir_len;
            }
        }
        /* Do we have class of device? */
        if (bufferLen >= index) {
            uint8_t cod_len = buffer[index++];
            if (cod_len > 0 && bufferLen >= (index + cod_len + 1)) {
                dev->cod_str = malloc(sizeof(char) * (cod_len + 1));
                if (dev->cod_str == NULL) {
                    // TODO: Panic. For now just skip the field
                } else {
                    memcpy(dev->cod_str, buffer + index, cod_len + 1);
                }
                index += (cod_len + 1);
            }
        }
        // TODO: Services to go here

        if (buffer[index] != '\n') {
            // TODO: Panic & recover
        }
        ++index;

        /* Does this device already exist in bt_devices[]? */
        if (bt_device_exists(dev)) {
            wendigo_update_bt_device(dev);
        } else {
            wendigo_add_bt_device(dev);
        }
    }
    UNUSED(app);
    return index;
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
    char *versionStr = realloc(wendigo_popup_text, sizeof(char) * bufferLen);
    if (versionStr == NULL) {
        // TODO: Panic
        // For now just consume this message
        return bytes_contains_newline(buffer, bufferLen);
    }
    wendigo_popup_text = versionStr;
    int i = 0;
    for (; i < bufferLen && buffer[i] != '\n'; ++i) {
        wendigo_popup_text[i] = buffer[i];
    }
    // TODO: Consider handling a missing newline more elegantly - Although it's currently
    //       the end-of-transmission marker so not possible to get here without one
    if (i == bufferLen) {
        --i; // Replace the last byte with NULL. This should never happen.
    }
    wendigo_popup_text[i] = '\0';
    wendigo_display_popup(app, "ESP32 Version", wendigo_popup_text);
    return i + 1;
}

/* When the end of a transmission is reached this function is called to parse the
   buffer and take the appropriate action. We expect to see one of the following:
    * Begin with 0xFF,0xFF,0xFF,0xFF,0xAA,0xAA,0xAA,0xAA: Bluetooth packet
    * Begin with 0x99,0x99,0x99,0x99,0x11,0x11,0x11,0x11: WiFi packet
    * Begin with "Wendigo "                              : Version message
*/
void parseBuffer(WendigoApp *app) {
    uint16_t consumedBytes = 0;
    if (bufferLen >= 8 && !memcmp(PREAMBLE_BT_BLE, buffer, PREAMBLE_LEN)) {
        consumedBytes = parseBufferBluetooth(app);
    } else if (bufferLen >= 8 && !memcmp(PREAMBLE_WIFI, buffer, PREAMBLE_LEN)) {
        consumedBytes = parseBufferWifi(app);
    } else if (bufferLen >= 8 && !memcmp(PREAMBLE_VER, buffer, PREAMBLE_LEN)) {
        consumedBytes = parseBufferVersion(app);
    } else {
        /* Extraneous content - Remove everything up to and including the first newline */
        for (consumedBytes = 0; consumedBytes < bufferLen && buffer[consumedBytes] != '\n'; ++consumedBytes) {}
        if (consumedBytes < bufferLen) {
            /* Found a newline at buffer[consumedBytes] - Also consume the newline */
            ++consumedBytes;
        } else {
            /* No newline found. This should never occur with the existing implementation */
            consumedBytes = 0;
        }
    }
    if (consumedBytes == 0) {
        // That was a bit of a waste
        // TODO: Diagnose why, communicate something useful
        return;
    }
    /* Remove `consumedBytes` bytes from the beginning of the buffer */
    memset(buffer, '\0', consumedBytes);
    /* Copy memory into buffer from buffer+consumedBytes to buffer+bufferLen.
       This should be OK when the buffer is exhausted because it copies 0 bytes */
    memcpy(buffer, buffer + consumedBytes, bufferLen - consumedBytes);
    /* Null what remains */
    memset(buffer + bufferLen - consumedBytes, '\0', consumedBytes);
    /* Buffer is now bufferLen - consumedBytes */
    bufferLen -= consumedBytes;
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
    uint16_t newline = bytes_contains_newline(buffer, bufferLen);
    while (newline != bufferLen) {
        parseBuffer(app);
        newline = bytes_contains_newline(buffer, bufferLen);
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
