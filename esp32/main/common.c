#include "common.h"

/* Storage to maintain a cache of recently-displayed devices */
uint16_t devices_count = 0;
uint16_t devices_capacity = 0;
wendigo_device *devices;

/** Banner width when in interactive mode */
uint8_t BANNER_WIDTH = 62;

/** Retrieve the specified MAC address. mac[] must be an initialised
 * byte array of length MAC_BYTES (6).
 */
esp_err_t wendigo_get_mac(WendigoMAC type, uint8_t mac[MAC_BYTES]) {
    if (type == WENDIGO_MACS_COUNT || !memcmp(mac, nullMac, MAC_BYTES)) {
        ESP_LOGE(TAG, "wendigo_get_mac() called with invalid arguments.");
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t ifType = type;
    if (type == WENDIGO_MAC_BASE) {
        /* This is 5 but I want to be able to use WENDIGO_MACS_COUNT */
        ifType = ESP_MAC_BASE;
    }
    return esp_read_mac(mac, ifType);
}

/** Set the specified MAC address to the specified value.
 * Permitted values are defined by the WendigoMAC enum. ESP32 supports
 * separate MACs for ethernet, STA and AP. Currently Wendigo only
 * uses the device in SoftAP mode.
 * Returns ESP_OK on success.
 */
esp_err_t wendigo_set_mac(WendigoMAC type, uint8_t mac[MAC_BYTES]) {
    if (type == WENDIGO_MACS_COUNT || !memcmp(mac, nullMac, MAC_BYTES)) {
        ESP_LOGE(TAG, "wendigo_set_mac() called with invalid arguments.");
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t ifType;
    switch (type) {
        case WENDIGO_MAC_WIFI:
            ifType = ESP_MAC_WIFI_SOFTAP;
            break;
        case WENDIGO_MAC_BLUETOOTH:
            ifType = ESP_MAC_BT;
            break;
        case WENDIGO_MAC_BASE:
        default:
            ifType = ESP_MAC_BASE;
            break;
    }
    /* Only change MAC if the specified interface is supported */
    uint8_t supported = wendigo_supported_features();
    if ((ifType == ESP_MAC_BT && ((supported & HW_BLE_SUPPORTED) != HW_BLE_SUPPORTED) &&
            ((supported & HW_BT_CLASSIC_SUPPORTED) != HW_BT_CLASSIC_SUPPORTED)) ||
            (ifType == ESP_MAC_WIFI_SOFTAP &&
            ((supported & HW_WIFI_24_SUPPORTED) != HW_WIFI_24_SUPPORTED) &&
            ((supported & HW_WIFI_5_SUPPORTED) != HW_WIFI_5_SUPPORTED))) {
        ESP_LOGE(TAG, "wendigo_set_mac(): This chip does not supported %s.",
            (ifType == ESP_MAC_BT) ? "Bluetooth" : "WiFi");
        return ESP_ERR_NOT_SUPPORTED;
    }
    return esp_iface_mac_addr_set(mac, ifType);
}

/** Send a Wendigo packet containing the specified MACs.
 * Packet format is:
 * * Preamble (4 bytes)
 * * Interface count (1 byte). For each interface:
 *   * Type (1 byte, WENDIGO_MAC_WIFI or WENDIGO_MAC_BLUETOOTH)
 *   * MAC (6 bytes)
 * * Terminator (4 bytes)
 */
esp_err_t wendigo_display_mac_uart(uint8_t wifi[MAC_BYTES], uint8_t bda[MAC_BYTES]) {
    uint8_t packet_len = WENDIGO_OFFSET_MAC_TERMINATOR + PREAMBLE_LEN + 1;
    uint8_t *packet = malloc(packet_len);
    if (packet == NULL) {
        return outOfMemory();
    }
    uint8_t supported = wendigo_supported_features();
    /* Count the number of interfaces supported by this chip */
    uint8_t supportedCount = 0;
    if (((supported & HW_BLE_SUPPORTED) == HW_BLE_SUPPORTED) ||
            ((supported & HW_BT_CLASSIC_SUPPORTED) == HW_BT_CLASSIC_SUPPORTED)) {
        ++supportedCount;
    }
    if (((supported & HW_WIFI_24_SUPPORTED) == HW_WIFI_24_SUPPORTED) ||
            ((supported & HW_WIFI_5_SUPPORTED) == HW_WIFI_5_SUPPORTED)) {
        ++supportedCount;
    }
    // TODO: Include base MAC later
    memcpy(packet, PREAMBLE_MAC, PREAMBLE_LEN);
    packet[WENDIGO_OFFSET_MAC_IF_COUNT] = supportedCount;
    /* Set up an offset to step through the rest of the packet because we might
     * have one or two MACs to send. */
    uint8_t offset = WENDIGO_OFFSET_MAC_IF_COUNT + 1;
    /* Is Bluetooth supported? */
    if (((supported & HW_BLE_SUPPORTED) == HW_BLE_SUPPORTED) ||
            ((supported & HW_BT_CLASSIC_SUPPORTED) == HW_BT_CLASSIC_SUPPORTED)) {
        packet[offset++] = (uint8_t)WENDIGO_MAC_BLUETOOTH;
        memcpy(packet + offset, bda, MAC_BYTES);
        offset += MAC_BYTES;
    }
    /* Is WiFi supported? */
    if (((supported & HW_WIFI_24_SUPPORTED) == HW_WIFI_24_SUPPORTED) ||
            ((supported & HW_WIFI_5_SUPPORTED) == HW_WIFI_5_SUPPORTED)) {
        packet[offset++] = (uint8_t)WENDIGO_MAC_WIFI;
        memcpy(packet + offset, wifi, MAC_BYTES);
        offset += MAC_BYTES;
    }
    memcpy(packet + offset, PACKET_TERM, PREAMBLE_LEN);
    /* Send the packet */
    esp_err_t result = ESP_OK;
    if (xSemaphoreTake(uartMutex, portMAX_DELAY)) {
        send_bytes(packet, offset);
        xSemaphoreGive(uartMutex);
    } else {
        result = ESP_ERR_INVALID_STATE;
    }
    free(packet);
    return result;
}

/** Display the device's MAC addresses to an interactive console */
esp_err_t wendigo_display_mac_interactive(uint8_t wifi[MAC_BYTES], uint8_t bda[MAC_BYTES]) {
    char *macStr = malloc(sizeof(char) * (MAC_STRLEN + 1));
    if (macStr == NULL) {
        return outOfMemory();
    }
    esp_err_t result = ESP_OK;
    uint8_t supported = wendigo_supported_features();
    print_star(BANNER_WIDTH, true);
    print_empty_row(BANNER_WIDTH);
    print_row_start(7);
    /* Only display MACs if the chip supports the interface */
    if (((supported & HW_BT_CLASSIC_SUPPORTED) == HW_BT_CLASSIC_SUPPORTED) ||
            ((supported & HW_BLE_SUPPORTED) == HW_BLE_SUPPORTED)) {
        result |= mac_bytes_to_string(bda, macStr);
    } else {
        snprintf(macStr, MAC_STRLEN + 1, "Not Supported.");
    }
    printf("Bluetooth Device Address: %20s", macStr);
    print_row_end(7);
    print_row_start(7);
    /* Check whether WiFi is supported */
    if (((supported & HW_WIFI_24_SUPPORTED) == HW_WIFI_24_SUPPORTED) ||
            ((supported & HW_WIFI_5_SUPPORTED) == HW_WIFI_5_SUPPORTED)) {
        result |= mac_bytes_to_string(wifi, macStr);
    } else {
        snprintf(macStr, MAC_STRLEN + 1, "Not Supported.");
    }
    printf("WiFi Access Point: %27s", macStr);
    print_row_end(7);
    print_empty_row(BANNER_WIDTH);
    print_star(BANNER_WIDTH, true);
    return result;
}

/** Displays ESP32's WiFi and Bluetooth MACs as long as the ESP32 supports
 * both. Otherwise only supported MACs will be displayed. */
esp_err_t wendigo_display_mac() {
    uint8_t wifi[MAC_BYTES];
    uint8_t bda[MAC_BYTES];
    esp_err_t result = ESP_OK;
    /* Check hardware support for WiFi & Bluetooth before trying to get its MAC */
    uint8_t supportedFeatures = wendigo_supported_features();
    /* Get the MAC if the chip supports WiFi */
    if (((supportedFeatures & HW_WIFI_24_SUPPORTED) == HW_WIFI_24_SUPPORTED) ||
            ((supportedFeatures & HW_WIFI_5_SUPPORTED) == HW_WIFI_5_SUPPORTED)) {
        result |= wendigo_get_mac(WENDIGO_MAC_WIFI, wifi);
    } else {
        memcpy(wifi, nullMac, MAC_BYTES);
    }
    /* Get the BDA if the chip supports Bluetooth */
    if (((supportedFeatures & HW_BT_CLASSIC_SUPPORTED) == HW_BT_CLASSIC_SUPPORTED) ||
            ((supportedFeatures & HW_BLE_SUPPORTED) == HW_BLE_SUPPORTED)) {
        result |= wendigo_get_mac(WENDIGO_MAC_BLUETOOTH, bda);
    } else {
        memcpy(bda, nullMac, MAC_BYTES);
    }
    if (scanStatus[SCAN_INTERACTIVE] == ACTION_ENABLE) {
        return result || wendigo_display_mac_interactive(wifi, bda);
    } else {
        return result || wendigo_display_mac_uart(wifi, bda);
    }
}

/** Locates a device with the MAC of the specified device in devices[] cache.
 * Returns a pointer to the object in devices[] if found, NULL otherwise.
 */
wendigo_device *retrieve_device(wendigo_device *dev) {
    wendigo_device *result = NULL;
    uint16_t idx = 0;
    for (; idx < devices_count && memcmp(dev->mac, devices[idx].mac, MAC_BYTES); ++idx) {}
    if (idx < devices_count) {
        result = &(devices[idx]);
    }
    return result;
}

/** Finds the wendigo_device associated with the specified MAC
 *  Returns NULL if not found.
 */
wendigo_device *retrieve_by_mac(esp_bd_addr_t mac) {
    wendigo_device dev;
    memcpy(dev.mac, mac, ESP_BD_ADDR_LEN);
    return retrieve_device(&dev);
}

/** Check whether the provided wendigo_device is present in the specified array of
 * wendigo_device* objects. Matching is based on the device's MAC (i.e. dev->mac).
 * Returns the index of the matching device, or array_len if not found.
 */
uint16_t wendigo_device_index_of(wendigo_device *dev, wendigo_device **array, uint16_t array_len) {
    /* Don't proceed if invalid arguments were provided */
    if (dev == NULL || array == NULL || array_len == 0) {
        return array_len;
    }
    uint16_t idx = 0;
    for (; idx < array_len && memcmp(array[idx]->mac, dev->mac, MAC_BYTES); ++idx) { }
    return idx;
}

/** Check whether a wendigo_device with the specified MAC is present in the specified array
 * of wendigo_device* objects.
 * Returns the index of the matching device, or array_len if not found.
 */
uint16_t wendigo_device_index_of_mac(uint8_t mac[MAC_BYTES], wendigo_device **array, uint16_t array_len) {
    /* Don't proceed if invalid arguments were provided */
    if (mac == NULL || array == NULL || array_len == 0) {
        return array_len;
    }
    wendigo_device dev;
    memcpy(dev.mac, mac, MAC_BYTES);
    return wendigo_device_index_of(&dev, array, array_len);
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
}

uint8_t wendigo_index_of_string(char *str, char **array, uint8_t array_len) {
    // TODO: Can I merge this with the above function?
    if (str == NULL || array == NULL || array_len == 0 || str[0] == '\0') {
        return array_len;
    }
    uint8_t idx = 0;
    for (; idx < array_len && (array[idx] == NULL || strncasecmp(str, array[idx], MAX_SSID_LEN)); ++idx) { }
    return idx;
}

/** Create and return an initialised wendigo_device pointer */
wendigo_device *wendigo_new_device(uint8_t *mac) {
    wendigo_device *device = malloc(sizeof(wendigo_device));
    if (device == NULL) {
        outOfMemory();
        return NULL;
    }
    explicit_bzero(device, sizeof(wendigo_device));
    device->tagged = false;
    gettimeofday(&(device->lastSeen), NULL);
    if (mac != NULL) {
        memcpy(device->mac, mac, MAC_BYTES);
    }
    return device;
}

/** Create and return a new WiFi Station object. mac may be NULL. */
wendigo_device *wendigo_new_sta(uint8_t *mac) {
    wendigo_device *result = wendigo_new_device(mac);
    if (result != NULL) {
        result->scanType = SCAN_WIFI_STA;
    }
    return result;
}

/** Create and return a new Access Point object. mac may be NULL. */
wendigo_device *wendigo_new_ap(uint8_t *mac) {
    wendigo_device *result = wendigo_new_device(mac);
    if (result != NULL) {
        result->scanType = SCAN_WIFI_AP;
        result->radio.ap.authmode = WIFI_AUTH_MAX;
    }
    return result;
}

/** Adds the specified device to devices[] if not already present.
 *  Updates the attributes of the specified device in devices[]
 *  if it already exists. */
esp_err_t add_device(wendigo_device *dev) {
    esp_err_t result = ESP_OK;
    wendigo_device *existingDevice = retrieve_device(dev);
    if (existingDevice == NULL) {
        /* Device not found - add it to devices[] */
        if (devices_count == devices_capacity) {
            /* No spare array capacity - malloc more */
            wendigo_device *new_devices = realloc(devices, sizeof(wendigo_device) * (devices_capacity + 10));
            if (new_devices != NULL) {
                devices_capacity += 10;
                devices = new_devices;
            } // Ignoring realloc() failure because we can still transmit `dev` to FZ
        }
        /* Check again whether we have capacity because the malloc might have failed */
        if (devices_count < devices_capacity) {
            /* Copy the entire block of memory containing `dev` into `devices[devices_count]` */
            memcpy(&(devices[devices_count]), dev, sizeof(wendigo_device));
            gettimeofday(&(devices[devices_count].lastSeen), NULL);
            if (dev->scanType == SCAN_HCI || dev->scanType == SCAN_BLE) {
                /* Duplicate bdname and eir if they exist so the caller can call free() */
                if (dev->radio.bluetooth.bdname_len > 0) {
                    devices[devices_count].radio.bluetooth.bdname = malloc(dev->radio.bluetooth.bdname_len + 1);
                    if (devices[devices_count].radio.bluetooth.bdname == NULL) {
                        result = outOfMemory();
                    } else {
                        memcpy(devices[devices_count].radio.bluetooth.bdname,
                               dev->radio.bluetooth.bdname, dev->radio.bluetooth.bdname_len);
                        devices[devices_count].radio.bluetooth.bdname[dev->radio.bluetooth.bdname_len] = '\0';
                        devices[devices_count].radio.bluetooth.bdname_len = dev->radio.bluetooth.bdname_len;
                    }
                }
                if (dev->radio.bluetooth.eir_len > 0) {
                    devices[devices_count].radio.bluetooth.eir = malloc(dev->radio.bluetooth.eir_len);
                    if (devices[devices_count].radio.bluetooth.eir == NULL) {
                        result = outOfMemory();
                    } else {
                        memcpy(devices[devices_count].radio.bluetooth.eir,
                               dev->radio.bluetooth.eir, dev->radio.bluetooth.eir_len);
                        devices[devices_count].radio.bluetooth.eir_len = dev->radio.bluetooth.eir_len;
                    }
                }
            } else if (dev->scanType == SCAN_WIFI_AP) {
                /* Duplicate dev->radio.ap.stations - While its *contents* don't need special
                   attention because they're pointers to existing devices, the array itself was
                   declared by - and will be freed by - a separate function. */
                   devices[devices_count].radio.ap.stations = NULL;
                   devices[devices_count].radio.ap.stations_count = 0;
                if (dev->radio.ap.stations != NULL && dev->radio.ap.stations_count > 0) {
                    devices[devices_count].radio.ap.stations = malloc(sizeof(uint8_t *) * dev->radio.ap.stations_count);
                    if (devices[devices_count].radio.ap.stations != NULL) {
                        /* If allocation fails all we need to do is ensure that stations_count is 0 -
                           linked stations will still be sent to Flipper, we just don't have capacity
                           to store them. stations_count was initialised to 0 above, so we're only
                           looking at the success case.
                        */
                        for (uint16_t i = 0; i < dev->radio.ap.stations_count; ++i) {
                            devices[devices_count].radio.ap.stations[i] = malloc(MAC_BYTES);
                            if (devices[devices_count].radio.ap.stations[i] != NULL) {
                                memcpy(devices[devices_count].radio.ap.stations[i], dev->radio.ap.stations[i], MAC_BYTES);
                            }
                        }
                        devices[devices_count].radio.ap.stations_count = dev->radio.ap.stations_count;
                    }
                }
            } else if (dev->scanType == SCAN_WIFI_STA) {
                /* Copy dev->radio.sta.saved_networks[] */
                if (dev->radio.sta.saved_networks_count == 0) {
                    devices[devices_count].radio.sta.saved_networks = NULL;
                } else {
                    devices[devices_count].radio.sta.saved_networks = malloc(
                        sizeof(char *) * dev->radio.sta.saved_networks_count);
                    if (devices[devices_count].radio.sta.saved_networks == NULL) {
                        devices[devices_count].radio.sta.saved_networks_count = 0;
                        // TODO: Error message
                    } else {
                        /* We have the array, copy the SSIDs */
                        uint8_t ssid_len;
                        for (uint8_t i = 0; i < dev->radio.sta.saved_networks_count; ++i) {
                            if (dev->radio.sta.saved_networks[i] != NULL) {
                                ssid_len = strlen(dev->radio.sta.saved_networks[i]);
                                devices[devices_count].radio.sta.saved_networks[i] = malloc(
                                    sizeof(char) * (ssid_len + 1));
                                if (devices[devices_count].radio.sta.saved_networks[i] != NULL) {
                                    strncpy(devices[devices_count].radio.sta.saved_networks[i],
                                        dev->radio.sta.saved_networks[i], ssid_len);
                                    devices[devices_count].radio.sta.saved_networks[i][ssid_len] = '\0';
                                }
                            }
                        }
                    }
                }
            }
            ++devices_count;
        }
    } else {
        /* Device exists. Update RSSI, lastSeen, and anything else that has changed */
        existingDevice->rssi = dev->rssi;
        gettimeofday(&(existingDevice->lastSeen), NULL);
        existingDevice->scanType = dev->scanType;
        if (dev->scanType == SCAN_HCI || dev->scanType == SCAN_BLE) {
            if (dev->radio.bluetooth.bdname_len > 0) {
                if (existingDevice->radio.bluetooth.bdname_len > 0 &&
                        existingDevice->radio.bluetooth.bdname != NULL) {
                    free(existingDevice->radio.bluetooth.bdname);
                    existingDevice->radio.bluetooth.bdname_len = 0;
                }
                existingDevice->radio.bluetooth.bdname = malloc(sizeof(char) * (dev->radio.bluetooth.bdname_len + 1));
                if (existingDevice->radio.bluetooth.bdname == NULL) {
                    result = outOfMemory();
                } else {
                    memcpy(existingDevice->radio.bluetooth.bdname,
                           dev->radio.bluetooth.bdname, dev->radio.bluetooth.bdname_len);
                    existingDevice->radio.bluetooth.bdname[dev->radio.bluetooth.bdname_len] = '\0';
                    existingDevice->radio.bluetooth.bdname_len = dev->radio.bluetooth.bdname_len;
                }
            }
            if (dev->radio.bluetooth.eir_len > 0) {
                if (existingDevice->radio.bluetooth.eir_len > 0 &&
                        existingDevice->radio.bluetooth.eir != NULL) {
                    free(existingDevice->radio.bluetooth.eir);
                }
                existingDevice->radio.bluetooth.eir = malloc(dev->radio.bluetooth.eir_len);
                if (existingDevice->radio.bluetooth.eir == NULL) {
                    result = outOfMemory();
                } else {
                    memcpy(existingDevice->radio.bluetooth.eir,
                           dev->radio.bluetooth.eir, dev->radio.bluetooth.eir_len);
                    existingDevice->radio.bluetooth.eir_len = dev->radio.bluetooth.eir_len;
                }
            }
            if (dev->radio.bluetooth.cod != 0) {
                existingDevice->radio.bluetooth.cod = dev->radio.bluetooth.cod;
            }
            // TODO: Think about whether I should malloc memory for services or use previously-allocated memory
            //       bt_uuid **known_services will make malloc'ing more difficult
            // TODO: Decide how to merge services - Should probably be done with a separate function
        } else if (dev->scanType == SCAN_WIFI_AP) {
            existingDevice->radio.ap.channel = dev->radio.ap.channel;
            /* Check whether `dev` contains any stations not in `existingDevice` */
            if (dev->radio.ap.stations_count > 0) {
                /* Find stations in `dev` that aren't in `existingDevice` */
                uint8_t newStations = 0;
                if (existingDevice->radio.ap.stations_count == 0) {
                    newStations = dev->radio.ap.stations_count;
                } else {
                    for (uint16_t i = 0; i < dev->radio.ap.stations_count; ++i) {
                        if (wendigo_index_of(dev->radio.ap.stations[i],
                                existingDevice->radio.ap.stations,
                                existingDevice->radio.ap.stations_count) ==
                                existingDevice->radio.ap.stations_count) {
                            ++newStations;
                        }
                    }
                }
                if (newStations > 0) {
                    /* Expand existingDevice's stations[] */
                    uint8_t **new_stations = realloc(existingDevice->radio.ap.stations,
                        sizeof(uint8_t *) * (newStations + existingDevice->radio.ap.stations_count));
                    if (new_stations != NULL) {
                        /* Successfully resized existingDevice's stations[]. Copy new MACs across */
                        uint16_t staIdx = existingDevice->radio.ap.stations_count;
                        for (uint16_t i = 0; i < dev->radio.ap.stations_count; ++i) {
                            if (wendigo_index_of(dev->radio.ap.stations[i], new_stations,
                                    existingDevice->radio.ap.stations_count) ==
                                    existingDevice->radio.ap.stations_count) {
                                new_stations[staIdx] = malloc(MAC_BYTES);
                                if (new_stations[staIdx] != NULL) {
                                    memcpy(new_stations[staIdx], dev->radio.ap.stations[i], MAC_BYTES);
                                }
                                ++staIdx;
                            }
                        }
                        /* Copy into place (in case stations[] was moved to find enough contiguous space) */
                        existingDevice->radio.ap.stations = new_stations;
                        existingDevice->radio.ap.stations_count += newStations;
                    }
                }
            }
            strncpy(existingDevice->radio.ap.ssid, dev->radio.ap.ssid, MAX_SSID_LEN + 1);
            existingDevice->radio.ap.ssid[MAX_SSID_LEN] = '\0';
        } else if (dev->scanType == SCAN_WIFI_STA) {
            existingDevice->radio.sta.channel = dev->radio.sta.channel;
            memcpy(existingDevice->radio.sta.apMac, dev->radio.sta.apMac, MAC_BYTES);
            /* Add any SSIDs in saved_networks[] that aren't already there.
               First, count the number of new devices. */
            uint8_t new_networks = 0;
            uint8_t ssid_idx;
            for (uint8_t i = 0; i < dev->radio.sta.saved_networks_count; ++i) {
                /* Is dev->radio.sta.saved_networks[i] in existingDevice->radio.sta.saved_networks? */
                ssid_idx = wendigo_index_of_string(dev->radio.sta.saved_networks[i],
                    existingDevice->radio.sta.saved_networks,
                    existingDevice->radio.sta.saved_networks_count);
                if (ssid_idx == existingDevice->radio.sta.saved_networks_count) {
                    ++new_networks;
                }
            }
            new_networks += existingDevice->radio.sta.saved_networks_count;
            char **new_pnl = realloc(existingDevice->radio.sta.saved_networks, sizeof(char *) * new_networks);
            if (new_pnl != NULL) {
                /* Append new SSIDs from dev->radio.sta.saved_networks */
                uint8_t current_network = existingDevice->radio.sta.saved_networks_count;
                uint8_t ssid_len;
                for (uint8_t i = 0; i < dev->radio.sta.saved_networks_count &&
                        current_network < new_networks; ++i) {
                    ssid_idx = wendigo_index_of_string(dev->radio.sta.saved_networks[i],
                        new_pnl, existingDevice->radio.sta.saved_networks_count);
                    if (ssid_idx == existingDevice->radio.sta.saved_networks_count) {
                        /* Append the SSID */
                        ssid_len = strlen(dev->radio.sta.saved_networks[i]);
                        new_pnl[current_network] = malloc(sizeof(char) * (ssid_len + 1));
                        if (new_pnl[current_network] != NULL) {
                            strncpy(new_pnl[current_network], dev->radio.sta.saved_networks[i], ssid_len);
                            new_pnl[current_network][ssid_len] = '\0';
                            ++current_network;
                        }
                    }
                }
                existingDevice->radio.sta.saved_networks = new_pnl;
                existingDevice->radio.sta.saved_networks_count = new_networks;
            }
        }
    }
    return result;
}

esp_err_t free_device(wendigo_device *dev) {
    if (dev->scanType == SCAN_HCI || dev->scanType == SCAN_BLE) {
        if (dev->radio.bluetooth.bdname_len > 0 && dev->radio.bluetooth.bdname != NULL) {
            free(dev->radio.bluetooth.bdname);
        }
        if (dev->radio.bluetooth.eir_len > 0 && dev->radio.bluetooth.eir != NULL) {
            free(dev->radio.bluetooth.eir);
        }
    } else if (dev->scanType == SCAN_WIFI_AP) {
        if (dev->radio.ap.stations != NULL && dev->radio.ap.stations_count > 0) {
            for (uint16_t i = 0; i < dev->radio.ap.stations_count; ++i) {
                if (dev->radio.ap.stations[i] != NULL) {
                    free(dev->radio.ap.stations[i]);
                }
            }
            free(dev->radio.ap.stations);
            dev->radio.ap.stations = NULL;
            dev->radio.ap.stations_count = 0;
        }
    } else if (dev->scanType == SCAN_WIFI_STA) {
        if (dev->radio.sta.saved_networks_count > 0 && dev->radio.sta.saved_networks != NULL) {
            for (uint8_t i = 0; i < dev->radio.sta.saved_networks_count; ++i) {
                if (dev->radio.sta.saved_networks[i] != NULL) {
                    free(dev->radio.sta.saved_networks[i]);
                }
            }
            free(dev->radio.sta.saved_networks);
            dev->radio.sta.saved_networks = NULL;
            dev->radio.sta.saved_networks_count = 0;
        }
    }
    return ESP_OK;
}

/** Helper functions to simplify, and minimise memory use of, banners */
void print_star(int size, bool newline) {
    for (int i = 0; i < size; ++i) {
        putc('*', stdout);
    }
    if (newline) {
        putc('\n', stdout);
    }
}

void print_space(int size, bool newline) {
    for (int i = 0; i < size; ++i) {
        putc(' ', stdout);
    }
    if (newline) {
        putc('\n', stdout);
    }
}

void print_row_start(int spaces) {
    print_star(1, false);
    print_space(spaces, false);
}

void print_row_end(int spaces) {
    print_space(spaces, false);
    print_star(1, true);
}

void print_empty_row(int lineLength) {
    print_star(1, false);
    print_space(lineLength - 2, false);
    print_star(1, true);
}

/** Display a simple out of memory message and set error code */
esp_err_t outOfMemory() {
    printf("%s\n", STRING_MALLOC_FAIL);
    return ESP_ERR_NO_MEM;
}

/** General purpose byte to string convertor
 *  byteCount specifies the number of bytes to be converted to a string,
 *  commencing at bytes.
 *  string must be an initialised char[] containing sufficient space
 *  for the results.
 *  String length (including terminating \0) will be 3 * byteCount
 *  (standard formatting - 0F:AA:E5)
 */
esp_err_t wendigo_bytes_to_string(uint8_t *bytes, char *string, int byteCount) {
    esp_err_t err = ESP_OK;
    char temp[4];
    memset(string, '\0', (3 * byteCount));
    for (int i = 0; i < byteCount; ++i) {
        sprintf(temp, "%02X", bytes[i]);
        if (i < (byteCount - 1)) {
            /* If we're not printing the last byte append ':' */
            strcat(temp, ":");
        }
        strcat(string, temp);
    }
    return err;
}

/** Convert the specified byte array to a string representing
 *  a MAC address. strMac must be a pointer initialised to
 *  contain at least 18 bytes (MAC + '\0')
 */
   esp_err_t mac_bytes_to_string(uint8_t *bMac, char *strMac) {
    if (bMac == NULL || strMac == NULL ) {
        return ESP_ERR_INVALID_ARG;
    }
    return wendigo_bytes_to_string(bMac, strMac, MAC_BYTES);
}

/** Convert the specified string to a byte array
 *  bMac must be a pointer to 6 bytes of allocated memory
 */
   esp_err_t wendigo_string_to_bytes(char *strMac, uint8_t *bMac) {
    uint8_t nBytes = (strlen(strMac) + 1) / 3; /* Support arbitrary-length string */
        if (nBytes == 0) {
        ESP_LOGE(TAG, "mac_string_to_bytes() called with an invalid string - There are no bytes\n\t%s\tExpected format 0A:1B:2C:3D:4E:5F:60", strMac);
        return ESP_ERR_INVALID_ARG;
    }
    for (int i = 0; i < nBytes; ++i) {
        sscanf(strMac + 3 * i, "%2hhx", &bMac[i]);
    }
    return ESP_OK;
}

void repeat_bytes(uint8_t byte, uint8_t count) {
    for (int i = 0; i < count; ++i) {
        putc(byte, stdout);
    }
}

void send_bytes(uint8_t *bytes, uint8_t size) {
    for (uint8_t i = 0; i < size; ++i) {
        putc(bytes[i], stdout);
    }
    fflush(stdout);
}

void send_end_of_packet() {
    send_bytes(PACKET_TERM, PREAMBLE_LEN);
    fflush(stdout);
}

/** Check which device-specific features are supported and
 * return the sum of each feature's SupportedHardwareMask
 * value.
 */
uint8_t wendigo_supported_features() {
    uint8_t result = 0;
    #if defined(CONFIG_DECODE_UUIDS)
        result += HW_BT_UUID_DICTIONARY;
    #endif
    #if defined(CONFIG_BT_CLASSIC_ENABLED)
        result += HW_BT_CLASSIC_SUPPORTED;
    #endif
    #if defined(CONFIG_BT_BLE_ENABLED)
        result += HW_BLE_SUPPORTED;
    #endif
    #if defined(CONFIG_ESP_WIFI_ENABLED) || \
            defined(CONFIG_ESP_HOST_WIFI_ENABLED) || \
            defined(CONFIG_ESP32_WIFI_ENABLED)
        result += HW_WIFI_24_SUPPORTED;
    #endif
    // TODO: Where is 5G defined?
    return result;
}