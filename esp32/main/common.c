#include "common.h"

/* Storage to maintain a cache of recently-displayed devices */
uint16_t devices_count = 0;
uint16_t devices_capacity = 0;
wendigo_device *devices;

/** Locates a device with the MAC of the specified device in devices[] cache.
 * Returns a pointer to the object in devices[] if found, NULL otherwise.
 */
wendigo_device *retrieve_device(wendigo_device *dev) {
    wendigo_device *result = NULL;
    int idx = 0;
    for (; idx < devices_count && memcmp(dev->mac, devices[idx].mac, ESP_BD_ADDR_LEN); ++idx) {}
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
                // No special cases
            } else if (dev->scanType == SCAN_WIFI_STA) {
                // No special cases
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
            /* Just reuse the stations pointer and free it later */
            existingDevice->radio.ap.stations = dev->radio.ap.stations;
            existingDevice->radio.ap.stations_count = dev->radio.ap.stations_count;
            strncpy((char *)existingDevice->radio.ap.ssid, (char *)dev->radio.ap.ssid, MAX_SSID_LEN + 1);
            existingDevice->radio.ap.ssid[MAX_SSID_LEN] = '\0';
        } else if (dev->scanType == SCAN_WIFI_STA) {
            existingDevice->radio.sta.channel = dev->radio.sta.channel;
            existingDevice->radio.sta.ap = dev->radio.sta.ap;
            memcpy(existingDevice->radio.sta.apMac, dev->radio.sta.apMac, MAC_BYTES);
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
            free(dev->radio.ap.stations);
            dev->radio.ap.stations = NULL;
            dev->radio.ap.stations_count = 0;
        }
    } else if (dev->scanType == SCAN_WIFI_STA) {
        dev->radio.sta.ap = NULL;
    }
    return ESP_OK;
}

/* Helper functions to simplify, and minimise memory use of, banners */
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

/* Display a simple out of memory message and set error code */
esp_err_t outOfMemory() {
    printf("%s\n", STRING_MALLOC_FAIL);
    return ESP_ERR_NO_MEM;
}

/* General purpose byte to string convertor
   byteCount specifies the number of bytes to be converted to a string,
   commencing at bytes.
   string must be an initialised char[] containing sufficient space
   for the results.
   String length (including terminating \0) will be 3 * byteCount
   (standard formatting - 0F:AA:E5)
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

/* Convert the specified byte array to a string representing
   a MAC address. strMac must be a pointer initialised to
   contain at least 18 bytes (MAC + '\0') */
   esp_err_t mac_bytes_to_string(uint8_t *bMac, char *strMac) {
    if (bMac == NULL || strMac == NULL ) {
        return ESP_ERR_INVALID_ARG;
    }
    return wendigo_bytes_to_string(bMac, strMac, MAC_BYTES);
}

/* Convert the specified string to a byte array
   bMac must be a pointer to 6 bytes of allocated memory */
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
    for (int i = 0; i < size; ++i) {
        putc(bytes[i], stdout);
    }
}

void send_end_of_packet() {
    repeat_bytes(0xAA, 4);
    repeat_bytes(0xFF, 4);
}
