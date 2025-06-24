#include "wifi.h"

/* Array of channels that are to be included in channel hopping.
   At startup this is initialised to include all supported channels. */
uint16_t *channels = NULL;
uint8_t channels_count = 0;
uint8_t channel_index = 0;
// TODO: Refactor to support 5GHz channels if the device supports 5GHz channels
const uint8_t WENDIGO_SUPPORTED_24_CHANNELS_COUNT = 14;
const uint16_t WENDIGO_SUPPORTED_24_CHANNELS[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
const uint8_t WENDIGO_SUPPORTED_5_CHANNELS_COUNT = 31;
const uint16_t WENDIGO_SUPPORTED_5_CHANNELS[] = {32, 36, 40, 44, 48, 52, 56, 60, 64, 68, 96, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144, 149, 153, 157, 161, 165, 169, 173, 177};
long hop_millis = CONFIG_DEFAULT_HOP_MILLIS;
TaskHandle_t channelHopTask = NULL; /* Independent task for channel hopping */

bool WIFI_INITIALISED = false;
uint8_t BANNER_WIDTH = 62;
static const char *WIFI_TAG = "WiFi@Wendigo";

/* Local function declarations */
void create_hop_task_if_needed();
void channelHopCallback(void *pvParameter);

/* Override the default implementation so we can send arbitrary 802.11 packets */
esp_err_t ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2, int32_t arg3) {
    return ESP_OK;
}

esp_err_t display_wifi_ap_uart(wendigo_device *dev) {
    esp_err_t result = ESP_OK;
    if (dev->scanType != SCAN_WIFI_AP) {
        return ESP_ERR_INVALID_ARG;
    }
    /* Only need 1 byte for scanType, cast down from 4 */
    uint8_t scanType = (uint8_t)dev->scanType;
    /* Send RSSI as a string to avoid issues casting negative values between uint8_t and int8_t */
    char rssi[RSSI_LEN + 3]; /* +3 rather than 1 to avoid compiler errors - In theory int16_t can be up to 6 chars */
    memset(rssi, '\0', RSSI_LEN + 3);
    snprintf(rssi, RSSI_LEN + 3, "%d", dev->rssi);
    /* Send dev->tagged as 1 for true, 0 for false */
    uint8_t tagged = (dev->tagged) ? 1 : 0;
    /* Send the packet */
    repeat_bytes(0x99, 4);
    repeat_bytes(0x11, 4);
    send_bytes(&scanType, sizeof(uint8_t));
    send_bytes(dev->mac, MAC_BYTES);
    send_bytes((uint8_t *)&(dev->radio.ap.channel), sizeof(uint16_t));
    send_bytes((uint8_t *)rssi, RSSI_LEN);
    send_bytes((uint8_t *)&(dev->lastSeen), sizeof(struct timeval));
    send_bytes(&tagged, sizeof(uint8_t));
    uint8_t ssid_len = strnlen((char *)dev->radio.ap.ssid, MAX_SSID_LEN + 1);
    if (dev->radio.ap.ssid[0] == '\0') {
        ssid_len = 0;
    }
    send_bytes(&ssid_len, sizeof(uint8_t));
    send_bytes(&(dev->radio.ap.stations_count), sizeof(uint8_t));
    send_bytes(dev->radio.ap.ssid, MAX_SSID_LEN);
    for (uint8_t i = 0; i < dev->radio.ap.stations_count; ++i) {
        /* Send the MAC of each connected device */
        /* It should be impossible to get a NULL station, but cater for it anyway */
        if (dev->radio.ap.stations == NULL || dev->radio.ap.stations[i] == NULL) {
            send_bytes(nullMac, MAC_BYTES);
        } else {
            send_bytes(((wendigo_device *)dev->radio.ap.stations[i])->mac, MAC_BYTES);
        }
    }
    send_end_of_packet();
    return result;
}

esp_err_t display_wifi_ap_interactive(wendigo_device *dev) {
    esp_err_t result = ESP_OK;
    if (dev->scanType != SCAN_WIFI_AP) {
        return ESP_ERR_INVALID_ARG;
    }
    char macStr[MAC_STRLEN + 1];
    mac_bytes_to_string(dev->mac, macStr);

    print_star(BANNER_WIDTH, true);
    print_star(1, false);
    print_space(4, false);
    /* Can we fit device type, SSID, MAC and RSSI all on a single line? */
    uint8_t row_len = strlen(radioShortNames[dev->scanType]) + MAC_STRLEN + 25 + strlen((char *)dev->radio.ap.ssid);
    uint8_t current_len = strlen(radioShortNames[dev->scanType]) + 1 + strlen((char *)dev->radio.ap.ssid);
    /* Common elements */
    if (strlen((char *)dev->radio.ap.ssid) > 0) {
        printf("%s: %s ", radioShortNames[dev->scanType], dev->radio.ap.ssid);
        current_len += 2;   /* Colon and extra space */
        ++row_len;          /* Colon */
    } else {
        printf("%s %s", radioShortNames[dev->scanType], dev->radio.ap.ssid);
    }
    /* Calculate remaining line space */
    uint8_t space_left = BANNER_WIDTH - current_len - 10; /* 10 banner elements */

    if (row_len <= BANNER_WIDTH) {
        /* Display MAC and RSSI on current row - How much space before MAC? */
        /* (CA:FE:BA:BE:00:00) RSSI: -120 */
        row_len = MAC_STRLEN + 13;
        print_space(space_left - row_len, false);
        printf("(%s) RSSI: %4d", macStr, dev->rssi);
        print_space(4, false);
        print_star(1, true);
        print_star(1, false);
        /* I have 28 characters to display as a block of 6 & of 22, & want them centred.
           - I want 3 equal blocks of spaces, which should be (BANNER_WIDTH - 10 - 28) / 3.
        */
        space_left = (BANNER_WIDTH - 38) / 3; /* Check length of final block in case of rounding */
        print_space(4 + space_left, false);
        printf("Ch. %2d", dev->radio.ap.channel); // TODO: Make space for an additional character, for 5GHz channels
        print_space(space_left, false);
        printf("%3d Stations Connected", dev->radio.ap.stations_count);
        row_len = 38 + (2 * space_left);
        print_space(4 + BANNER_WIDTH - row_len, false);
        print_star(1, true);
    } else {
        /* Just display RSSI on this row */
        row_len = 10; /* RSSI: -120 */
        print_space(space_left - row_len, false);
        printf("RSSI: %4d", dev->rssi);
        print_space(4, false);
        print_star(1, true);
        print_star(1, false);
        print_space(4, false);
        /* Display (00:CA:FE:BA:BE:00) Ch. 11 100 Stations Connected
           Fully justified - 2 blocks of space centreing channel */
        space_left = (BANNER_WIDTH - MAC_STRLEN - 40) / 2;
        printf("(%s)", macStr);
        print_space(space_left, false);
        printf("Ch. %2d", dev->radio.ap.channel); // TODO: Make space for a additional character, for 5GHz channels        row_len = MAC_STRLEN + 40 + space_left;
        print_space(BANNER_WIDTH - row_len, false);
        printf("%3d Stations Connected", dev->radio.ap.stations_count);
        print_space(4, false);
        print_star(1, true);
    }
    print_star(BANNER_WIDTH, true);
    return result;
}

esp_err_t display_wifi_sta_uart(wendigo_device *dev) {
    esp_err_t result = ESP_OK;
    if (dev->scanType != SCAN_WIFI_STA) {
        return ESP_ERR_INVALID_ARG;
    }
    /* scanType only needs 1 byte, cast down from 4 */
    uint8_t scanType = (uint8_t)dev->scanType;
    /* Send RSSI as a string to avoid casting to and from uint8_t and int8_t */
    char rssi[RSSI_LEN + 3]; /* +3 rather than 1 to avoid compiler errors - int16_t can be up to 6 chars */
    memset(rssi, '\0', RSSI_LEN + 3);
    snprintf(rssi, RSSI_LEN + 3, "%d", dev->rssi);
    /* Send tagged as 1 for true, 0 for false */
    uint8_t tagged = (dev->tagged) ? 1 : 0;
    /* Send the packet */
    repeat_bytes(0x99, 4);
    repeat_bytes(0xFF, 4);
    send_bytes(&scanType, sizeof(uint8_t));
    send_bytes(dev->mac, MAC_BYTES);
    send_bytes((uint8_t *)&(dev->radio.sta.channel), sizeof(uint16_t));
    send_bytes((uint8_t *)rssi, RSSI_LEN);
    send_bytes((uint8_t *)&(dev->lastSeen), sizeof(struct timeval));
    send_bytes(&tagged, sizeof(uint8_t));
    send_bytes(dev->radio.sta.apMac, MAC_BYTES);
    uint8_t ssid_len = 0;
    char *ssid = NULL;
    if (dev->radio.sta.ap != NULL) {
        ssid = (char *)(((wendigo_device *)dev->radio.sta.ap)->radio.ap.ssid);
        ssid_len = strlen(ssid);
    }
    send_bytes(&ssid_len, sizeof(uint8_t));
    send_bytes((uint8_t *)ssid, MAX_SSID_LEN); /* Changed to fixed-length SSID */
    send_end_of_packet();
    return result;
}

esp_err_t display_wifi_sta_interactive(wendigo_device *dev) {
    esp_err_t result = ESP_OK;
    if (dev->scanType != SCAN_WIFI_STA) {
        return ESP_ERR_INVALID_ARG;
    }
    char macStr[MAC_STRLEN + 1];
    mac_bytes_to_string(dev->mac, macStr);
    
    print_star(BANNER_WIDTH, true);
    print_star(1, false);
    print_space(4, false);
    printf("%s: %s", radioShortNames[dev->scanType], macStr);
    /* Calculate size of space blocks - 2 equally-sized block */
    uint8_t space_len = (BANNER_WIDTH - strlen(radioShortNames[dev->scanType]) - MAC_STRLEN - 28) / 2;
    print_space(space_len, false);
    printf("Ch. %2d", dev->radio.sta.channel); // TODO: Make space for an additional character, for 5GHz channels
    /* Cater for rounding in space_len */
    uint8_t row_len = strlen(radioShortNames[dev->scanType]) + MAC_STRLEN + 28 + space_len;
    print_space(BANNER_WIDTH - row_len, false);
    printf("RSSI: %4d", dev->rssi);
    print_space(4, false);
    print_star(1, true);
    /* Display AP info on next row (if available) */
    print_star(1, false);
    print_space(4, false);
    if (dev->radio.sta.ap != NULL) {
        char apMacStr[MAC_STRLEN + 1];
        mac_bytes_to_string(dev->radio.sta.apMac, apMacStr);
        char ssid[MAX_SSID_LEN + 1];
        strncpy(ssid, (char *)((wendigo_device *)dev->radio.sta.ap)->radio.ap.ssid, MAX_SSID_LEN + 1);
        ssid[MAX_SSID_LEN] = '\0';
        uint8_t ssid_max_len = BANNER_WIDTH - MAC_STRLEN - 19;
        if (strlen(ssid) > ssid_max_len) {
            /* Truncate SSID and add ellipses */
            for (uint8_t i = ssid_max_len - 3; i < ssid_max_len; ++i) {
                ssid[i] = '.';
            }
            ssid[ssid_max_len] = '\0';
        }
        space_len = BANNER_WIDTH - strlen(ssid) - MAC_STRLEN - 18;
        printf("AP: \"%s\"", ssid);
        print_space(space_len, false);
        printf("(%s)", apMacStr);
        print_space(4, false);
    } else {
        space_len = (BANNER_WIDTH - strlen("AP Not Yet Found") - 10) / 2;
        print_space(space_len, false);
        printf("AP Not Yet Found");
        print_space(BANNER_WIDTH - strlen("AP Not Yet Found") - space_len - 6, false);
    }
    print_star(1, true);
    print_star(BANNER_WIDTH, true);
    return result;
}

esp_err_t display_wifi_device(wendigo_device *dev, bool force_display) {
    /* If Focus Mode is enabled only display the device if it's tagged */
    if (dev == NULL) {
        return ESP_OK; /* Not an error */
    }
    wendigo_device *existing_device = retrieve_device(dev);
    if (scanStatus[SCAN_FOCUS] == ACTION_ENABLE && (existing_device == NULL || !existing_device->tagged)) {
        return ESP_OK;
    }
    if (scanStatus[SCAN_INTERACTIVE] == ACTION_ENABLE) {
        /* If device throttling is enabled make sure sufficient time has passed before displaying it */
        if (CONFIG_DELAY_AFTER_DEVICE_DISPLAYED > 0 && !force_display) {
            wendigo_device *existingDevice = retrieve_device(dev);
            if (existingDevice != NULL) {
                /* We've seen the device before, have we seen it too recently? */
                struct timeval nowTime;
                gettimeofday(&nowTime, NULL);
                double elapsed = (nowTime.tv_sec - existingDevice->lastSeen.tv_sec) * 1000;
                if (elapsed < CONFIG_DELAY_AFTER_DEVICE_DISPLAYED) {
                    return ESP_OK;
                }
            }
        }
        if (dev->scanType == SCAN_WIFI_AP) {
            return display_wifi_ap_interactive(dev);
        } else if (dev->scanType == SCAN_WIFI_STA) {
            return display_wifi_sta_interactive(dev);
        }
    } else {
        if (dev->scanType == SCAN_WIFI_AP) {
            return display_wifi_ap_uart(dev);
        } else if (dev->scanType == SCAN_WIFI_STA) {
            return display_wifi_sta_uart(dev);
        }
    }
    /* We shouldn't reach this point - If we have there's something funky about `dev` */
    return ESP_ERR_INVALID_ARG;
}

/** Link the specified devices to reflect their association.
 */
esp_err_t set_associated(wendigo_device *sta, wendigo_device *ap) {
    if (sta == NULL || ap == NULL || sta->scanType != SCAN_WIFI_STA || ap->scanType != SCAN_WIFI_AP) {
        return ESP_ERR_INVALID_ARG;
    }
    sta->radio.sta.ap = ap;
    memcpy(sta->radio.sta.apMac, ap->mac, MAC_BYTES);
    /* See if sta is present in ap->radio.ap.stations */
    uint8_t i;
    for (i = 0; i < ap->radio.ap.stations_count && memcmp(sta->mac, ((wendigo_device *)ap->radio.ap.stations[i])->mac, MAC_BYTES); ++i) { }
    if (i == ap->radio.ap.stations_count) {
        /* Station not found in ap->radio.ap.stations - Add it */
        wendigo_device **new_stations = realloc(ap->radio.ap.stations, sizeof(wendigo_device *) * (ap->radio.ap.stations_count + 1));
        if (new_stations == NULL) {
            return outOfMemory();
        }
        new_stations[ap->radio.ap.stations_count++] = sta;
        ap->radio.ap.stations = (void **)new_stations;
        //ap->radio.ap.stations[ap->radio.ap.stations_count++] = sta;
    }
    return ESP_OK;
}

/** Parse a beacon frame and either create or update a wendigo_device for
 * the AP. As the only STA identifier we have is the MAC a STA device
 * will not be created.
 */
esp_err_t parse_beacon(uint8_t *payload, wifi_pkt_rx_ctrl_t rx_ctrl) {
    wendigo_device *dev = retrieve_by_mac(payload + BEACON_BSSID_OFFSET);
    bool creating = false;
    if (dev == NULL) {
        creating = true;
        dev = malloc(sizeof(wendigo_device));
        if (dev == NULL) {
            return outOfMemory();
        }
        memcpy(dev->mac, payload + BEACON_BSSID_OFFSET, MAC_BYTES);
        dev->tagged = false;
        dev->radio.ap.stations = NULL;
        dev->radio.ap.stations_count = 0;
        /* Null out SSID */
        memset(dev->radio.ap.ssid, '\0', sizeof(dev->radio.ap.ssid));
    }
    dev->scanType = SCAN_WIFI_AP;
    dev->rssi = rx_ctrl.rssi;
    dev->radio.ap.channel = rx_ctrl.channel;
    uint8_t ssid_len = payload[BEACON_SSID_OFFSET - 1];
    if (ssid_len > 0) {
        memcpy(dev->radio.ap.ssid, payload + BEACON_SSID_OFFSET, ssid_len);
        dev->radio.ap.ssid[ssid_len] = '\0';
    }

    esp_err_t result = ESP_OK;
    if (creating) {
        result = add_device(dev);
        free(dev);
    } else {
        gettimeofday(&(dev->lastSeen), NULL);
    }
    display_wifi_device(dev, creating);
    return result;
}

/** Parse a probe request frame, creating or updating a wendigo_device for
 * the STA that transmitted it.
 * TODO: Add network_list[] to wendigo_wifi_ap, add the probed SSID to this
 */
esp_err_t parse_probe_req(uint8_t *payload, wifi_pkt_rx_ctrl_t rx_ctrl) {
    wendigo_device *dev = retrieve_by_mac(payload + PROBE_SRCADDR_OFFSET);
    bool creating = false;
    if (dev == NULL) {
        creating = true;
        dev = malloc(sizeof(wendigo_device));
        if (dev == NULL) {
            return outOfMemory();
        }
        memcpy(dev->mac, payload + PROBE_SRCADDR_OFFSET, MAC_BYTES);
        dev->tagged = false;
        dev->radio.sta.ap = NULL;
        memcpy(dev->radio.sta.apMac, nullMac, MAC_BYTES);
    }
    dev->scanType = SCAN_WIFI_STA;
    dev->rssi = rx_ctrl.rssi;
    dev->radio.sta.channel = rx_ctrl.channel;
    // TODO: Add SSID to the STA's network list

    esp_err_t result = ESP_OK;
    if (creating) {
        result = add_device(dev);
        free(dev);
    } else {
        gettimeofday(&(dev->lastSeen), NULL);
    }
    display_wifi_device(dev, creating);
    return result;
}

/** Parse a probe response packet, creating or updating a wendigo_device
 * for both the source (AP) and destination (STA).
 */
esp_err_t parse_probe_resp(uint8_t *payload, wifi_pkt_rx_ctrl_t rx_ctrl) {
    wendigo_device *ap = retrieve_by_mac(payload + PROBE_RESPONSE_BSSID_OFFSET);
    wendigo_device *sta = NULL;
    bool creatingSta = false;
    bool creatingAp = false;
    esp_err_t result = ESP_OK;

    if (memcmp(broadcastMac, payload + PROBE_RESPONSE_DESTADDR_OFFSET, MAC_BYTES)) {
        /* Not a broadcast packet - it's being sent to an actual STA */
        sta = retrieve_by_mac(payload + PROBE_RESPONSE_DESTADDR_OFFSET);
        if (sta == NULL) {
            /* Create a wendigo_device for STA */
            creatingSta = true;
            sta = malloc(sizeof(wendigo_device));
            if (sta != NULL) {
                memcpy(sta->mac, payload + PROBE_RESPONSE_DESTADDR_OFFSET, MAC_BYTES);
                sta->tagged = false;
                sta->radio.sta.ap = NULL;   /* We'll update these later */
                memcpy(sta->radio.sta.apMac, nullMac, MAC_BYTES);
            }
        }
        /* If we weren't able to add the STA continue on to the AP */
        if (sta != NULL) {
            sta->scanType = SCAN_WIFI_STA;
            sta->radio.sta.channel = rx_ctrl.channel;
            if (creatingSta) {
                result |= add_device(sta);
                free(sta);
                /* Get a pointer to the actual object so we can set its AP later */
                sta = retrieve_by_mac(payload + PROBE_RESPONSE_DESTADDR_OFFSET);
            } else {
                gettimeofday(&(sta->lastSeen), NULL);
            }
        }
    }
    if (ap == NULL) {
        creatingAp = true;
        ap = malloc(sizeof(wendigo_device));
        if (ap == NULL) {
            return outOfMemory();
        }
        memcpy(ap->mac, payload + PROBE_RESPONSE_BSSID_OFFSET, MAC_BYTES);
        ap->tagged = false;
        ap->radio.ap.stations = NULL;   /* We'll update these later */
        ap->radio.ap.stations_count = 0;
        /* Null out SSID */
        memset(ap->radio.ap.ssid, '\0', sizeof(ap->radio.ap.ssid));
    }
    ap->scanType = SCAN_WIFI_AP;
    ap->rssi = rx_ctrl.rssi;
    ap->radio.ap.channel = rx_ctrl.channel;
    uint8_t ssid_len = payload[PROBE_RESPONSE_SSID_OFFSET - 1];
    if (ssid_len > 0) {
        memcpy(ap->radio.ap.ssid, payload + PROBE_RESPONSE_SSID_OFFSET, ssid_len);
        ap->radio.ap.ssid[ssid_len] = '\0';
    }
    if (creatingAp) {
        result |= add_device(ap);
        free(ap);
        ap = retrieve_by_mac(payload + PROBE_RESPONSE_BSSID_OFFSET);
    } else {
        gettimeofday(&(ap->lastSeen), NULL);
    }
    /* Even though this is a probe response, meaning the STA and AP have not successfully
       connected, link the AP and STA to each other because we don't currently parse enough
       packets to set the links upon successful association. */
    if (ap != NULL && sta != NULL) {
        result |= set_associated(sta, ap);
    }
    display_wifi_device(ap, creatingAp);
    display_wifi_device(sta, creatingSta);
    return result;
}

/** Parse an RTS (request to send) frame and create or update the
 * wendigo_device representing the transmitting STA and receiving AP.
 */
esp_err_t parse_rts(uint8_t *payload, wifi_pkt_rx_ctrl_t rx_ctrl) {
    wendigo_device *sta = retrieve_by_mac(payload + RTS_CTS_SRCADDR);
    wendigo_device *ap = retrieve_by_mac(payload + RTS_CTS_DESTADDR);
    bool creatingAp = false;
    bool creatingSta = false;
    esp_err_t result = ESP_OK;
    if (sta == NULL) {
        creatingSta = true;
        sta = malloc(sizeof(wendigo_device));
        if (sta == NULL) {
            /* No point continuing - The only AP info we have is MAC */
            return outOfMemory();
        }
        memcpy(sta->mac, payload + RTS_CTS_SRCADDR, MAC_BYTES);
        sta->tagged = false;
        sta->radio.sta.ap = NULL;
        memcpy(sta->radio.sta.apMac, nullMac, MAC_BYTES);
    }
    sta->rssi = rx_ctrl.rssi;
    sta->scanType = SCAN_WIFI_STA;
    sta->radio.sta.channel = rx_ctrl.channel;
    if (creatingSta) {
        result |= add_device(sta);
        free(sta);
        sta = retrieve_by_mac(payload + RTS_CTS_SRCADDR);
    } else {
        gettimeofday(&(sta->lastSeen), NULL);
    }
    if (ap == NULL) {
        /* Even though the only AP info we have is MAC, we also know
           that STA is connected to it - Create a wendigo_device for
           the AP so the connection can be recorded. */
           creatingAp = true;
           ap = malloc(sizeof(wendigo_device));
           if (ap == NULL) {
            return outOfMemory();
           }
           memcpy(ap->mac, payload + RTS_CTS_DESTADDR, MAC_BYTES);
           ap->tagged = false;
           /* Null out SSID */
           memset(ap->radio.ap.ssid, 0x00, sizeof(ap->radio.ap.ssid));
           ap->radio.ap.stations = NULL;
           ap->radio.ap.stations_count = 0;
    }
    ap->scanType = SCAN_WIFI_AP;
    ap->radio.ap.channel = rx_ctrl.channel;
    if (creatingAp) {
        result |= add_device(ap);
        free(ap);
        ap = retrieve_by_mac(payload + RTS_CTS_DESTADDR);
    } else {
        gettimeofday(&(ap->lastSeen), NULL);
    }
    /* Link the AP and STA */
    if (ap != NULL && sta != NULL) {
        result |= set_associated(sta, ap);
    }
    display_wifi_device(ap, creatingAp);
    display_wifi_device(sta, creatingSta);
    return result;
}

/** Parse a CTS (clear to send) packet and create or update the wendigo_device
 * objects representing the transmitting AP and receiving STA.
 */
esp_err_t parse_cts(uint8_t *payload, wifi_pkt_rx_ctrl_t rx_ctrl) {
    wendigo_device *sta = retrieve_by_mac(payload + RTS_CTS_DESTADDR);
    wendigo_device *ap = retrieve_by_mac(payload + RTS_CTS_SRCADDR);
    bool creatingAp = false;
    bool creatingSta = false;
    esp_err_t result = ESP_OK;

    if (ap == NULL) {
        /* Create the AP */
        creatingAp = true;
        ap = malloc(sizeof(wendigo_device));
        if (ap == NULL) {
            return outOfMemory();
        }
        memcpy(ap->mac, payload + RTS_CTS_SRCADDR, MAC_BYTES);
        ap->tagged = false;
        /* Null out SSID */
        memset(ap->radio.ap.ssid, '\0', sizeof(ap->radio.ap.ssid));
        ap->radio.ap.stations_count = 0;
        ap->radio.ap.stations = NULL;
    }
    ap->scanType = SCAN_WIFI_AP;
    ap->rssi = rx_ctrl.rssi;
    ap->radio.ap.channel = rx_ctrl.channel;
    if (creatingAp) {
        result |= add_device(ap);
        free(ap);
        ap = retrieve_by_mac(payload + RTS_CTS_SRCADDR);
    } else {
        gettimeofday(&(ap->lastSeen), NULL);
    }
    if (sta == NULL) {
        /* While the only thing we know about the STA is its MAC, create
           a wendigo_device for it so it can be linked to the AP. */
        creatingSta = true;
        sta = malloc(sizeof(wendigo_device));
        if (sta == NULL) {
            return outOfMemory();
        }
        memcpy(sta->mac, payload + RTS_CTS_DESTADDR, MAC_BYTES);
        sta->tagged = false;
        sta->radio.sta.ap = NULL;
        memcpy(sta->radio.sta.apMac, nullMac, MAC_BYTES);
    }
    sta->scanType = SCAN_WIFI_STA;
    sta->radio.sta.channel = rx_ctrl.channel;
    if (creatingSta) {
        result |= add_device(sta);
        free(sta);
        sta = retrieve_by_mac(payload + RTS_CTS_DESTADDR);
    } else {
        gettimeofday(&(sta->lastSeen), NULL);
    }
    /* Link the AP and STA */
    if (ap != NULL && sta != NULL) {
        set_associated(sta, ap);
    }
    display_wifi_device(ap, creatingAp);
    display_wifi_device(sta, creatingSta);
    return result;
}

/** Parse a data packet. These could go in either direction so we know the source
 * and destination MAC, but don't know which is an AP and which a STA. This function
 * will search for both MACs in the device cache to determine which is which. If
 * both devices are present in the cache they are linked.
 */
esp_err_t parse_data(uint8_t *payload, wifi_pkt_rx_ctrl_t rx_ctrl) {
    wendigo_device *src = retrieve_by_mac(payload + DEAUTH_SRCADDR_OFFSET);
    wendigo_device *dest = retrieve_by_mac(payload + DEAUTH_DESTADDR_OFFSET);

    if (src != NULL) {
        /* Found the transmitting device - Update its details */
        gettimeofday(&(src->lastSeen), NULL);
        src->rssi = rx_ctrl.rssi;
        if (src->scanType == SCAN_WIFI_AP) {
            src->radio.ap.channel = rx_ctrl.channel;
            if (dest != NULL && dest->scanType == SCAN_WIFI_STA) {
                set_associated(dest, src);
            }
        } else if (src->scanType == SCAN_WIFI_STA) {
            src->radio.sta.channel = rx_ctrl.channel;
            if (dest != NULL && dest->scanType == SCAN_WIFI_AP) {
                set_associated(src, dest);
            }
        }
    }
    if (dest != NULL) {
        /* Found the receiving device */
        gettimeofday(&(dest->lastSeen), NULL);
        if (dest->scanType == SCAN_WIFI_AP) {
            dest->radio.ap.channel = rx_ctrl.channel;
        } else if (dest->scanType == SCAN_WIFI_STA) {
            dest->radio.ap.channel = rx_ctrl.channel;
        }
    }
    display_wifi_device(src, false);
    display_wifi_device(dest, false);
    return ESP_OK;
}

/** Parse a deauth packet, creating or updating the wendigo_device representing
 * the transmitting AP and receiving STA (although the only STA information
 * we have is MAC and channel).
 */
esp_err_t parse_deauth(uint8_t *payload, wifi_pkt_rx_ctrl_t rx_ctrl) {
    wendigo_device *ap = retrieve_by_mac(payload + DEAUTH_BSSID_OFFSET);
    wendigo_device *sta = NULL;
    bool creatingAp = false;
    bool creatingSta = false;
    esp_err_t result = ESP_OK;
    /* Add or update the AP */
    if (ap == NULL) {
        creatingAp = true;
        ap = malloc(sizeof(wendigo_device));
        if (ap == NULL) {
            return outOfMemory();
        }
        memcpy(ap->mac, payload + DEAUTH_BSSID_OFFSET, MAC_BYTES);
        ap->tagged = false;
        /* Null out SSID */
        memset(ap->radio.ap.ssid, '\0', sizeof(ap->radio.ap.ssid));
        ap->radio.ap.stations = NULL;
        ap->radio.ap.stations_count = 0;
    }
    ap->scanType = SCAN_WIFI_AP;
    ap->rssi = rx_ctrl.rssi;
    ap->radio.ap.channel = rx_ctrl.channel;
    if (creatingAp) {
        result |= add_device(ap);
        free(ap);
        ap = retrieve_by_mac(payload + DEAUTH_BSSID_OFFSET);
    } else {
        gettimeofday(&(ap->lastSeen), NULL);
    }
    /* End now if this is a broadcast packet */
    if (!memcmp(payload + DEAUTH_DESTADDR_OFFSET, broadcastMac, MAC_BYTES)) {
        return result;
    }
    sta = retrieve_by_mac(payload + DEAUTH_DESTADDR_OFFSET);
    if (sta == NULL) {
        creatingSta = true;
        sta = malloc(sizeof(wendigo_device));
        if (sta == NULL) {
            return outOfMemory();
        }
        memcpy(sta->mac, payload + DEAUTH_DESTADDR_OFFSET, MAC_BYTES);
        sta->tagged = false;
        sta->radio.sta.ap = NULL;
        memcpy(sta->radio.sta.apMac, nullMac, MAC_BYTES);
    }
    sta->scanType = SCAN_WIFI_STA;
    sta->radio.sta.channel = rx_ctrl.channel;
    if (creatingSta) {
        result |= add_device(sta);
        free(sta);
        sta = retrieve_by_mac(payload + DEAUTH_DESTADDR_OFFSET);
    } else {
        gettimeofday(&(sta->lastSeen), NULL);
    }
    /* Link the AP and STA */
    if (ap != NULL && sta != NULL) {
        result |= set_associated(sta, ap);
    }
    display_wifi_device(ap, creatingAp);
    display_wifi_device(sta, creatingSta);
    return result;
}

/** Parse a disassociation packet, creating or updating the wendigo_device
 * representing the transmitting STA and receiving AP (although the only AP
 * information we have is MAC and channel).
 */
esp_err_t parse_disassoc(uint8_t *payload, wifi_pkt_rx_ctrl_t rx_ctrl) {
    wendigo_device *sta = retrieve_by_mac(payload + DEAUTH_SRCADDR_OFFSET);
    wendigo_device *ap = NULL;
    bool creatingAp = false;
    bool creatingSta = false;
    esp_err_t result = ESP_OK;
    /* Add or update the STA */
    if (sta == NULL) {
        creatingSta = true;
        sta = malloc(sizeof(wendigo_device));
        if (sta == NULL) {
            return outOfMemory();
        }
        memcpy(sta->mac, payload + DEAUTH_SRCADDR_OFFSET, MAC_BYTES);
        sta->tagged = false;
        sta->radio.sta.ap = NULL;
        memcpy(sta->radio.sta.apMac, nullMac, MAX_CANON);
    }
    sta->scanType = SCAN_WIFI_STA;
    sta->rssi = rx_ctrl.rssi;
    sta->radio.sta.channel = rx_ctrl.channel;
    if (creatingSta) {
        result |= add_device(sta);
        free(sta);
        sta = retrieve_by_mac(payload + DEAUTH_SRCADDR_OFFSET);
    } else {
        gettimeofday(&(sta->lastSeen), NULL);
    }
    /* End now if it's a broadcast packet */
    if (!memcmp(payload + DEAUTH_DESTADDR_OFFSET, broadcastMac, MAC_BYTES)) {
        return result;
    }
    /* Add or update the AP */
    ap = retrieve_by_mac(payload + DEAUTH_DESTADDR_OFFSET);
    if (ap == NULL) {
        creatingAp = true;
        ap = malloc(sizeof(wendigo_device));
        if (ap == NULL) {
            return outOfMemory();
        }
        memcpy(ap->mac, payload + DEAUTH_DESTADDR_OFFSET, MAC_BYTES);
        ap->tagged = false;
        /* Null out SSID */
        memset(ap->radio.ap.ssid, '\0', sizeof(ap->radio.ap.ssid));
        ap->radio.ap.stations = NULL;
        ap->radio.ap.stations_count = 0;
    }
    ap->scanType = SCAN_WIFI_AP;
    ap->radio.ap.channel = rx_ctrl.channel;
    if (creatingAp) {
        result |= add_device(ap);
        free(ap);
        ap = retrieve_by_mac(payload + DEAUTH_DESTADDR_OFFSET);
    } else {
        gettimeofday(&(ap->lastSeen), NULL);
    }
    /* Link the AP and STA */
    if (ap != NULL && sta != NULL) {
        result |= set_associated(sta, ap);
    }
    display_wifi_device(ap, creatingAp);
    display_wifi_device(sta, creatingSta);
    return result;
}

/* Monitor mode callback
   This is the callback function invoked when the wireless interface receives any selected packet.
*/
void wifi_pkt_rcvd(void *buf, wifi_promiscuous_pkt_type_t type) {
    wifi_promiscuous_pkt_t *data = (wifi_promiscuous_pkt_t *)buf;
    uint8_t *payload = data->payload;
    esp_err_t result = ESP_OK;
    /* Pass the packet to the relevant parser */
    switch (payload[0]) {
        case WIFI_FRAME_BEACON:
            result = parse_beacon(payload, data->rx_ctrl);
            break;
        case WIFI_FRAME_PROBE_REQ:
            result = parse_probe_req(payload, data->rx_ctrl);
            break;
        case WIFI_FRAME_PROBE_RESP:
            result = parse_probe_resp(payload, data->rx_ctrl);
            break;
        case WIFI_FRAME_DEAUTH:
            result = parse_deauth(payload, data->rx_ctrl);
            break;
        case WIFI_FRAME_DISASSOC:
            result = parse_disassoc(payload, data->rx_ctrl);
            break;
        case WIFI_FRAME_ASSOC_REQ:
            //
            break;
        case WIFI_FRAME_ASSOC_RESP:
            //
            break;
        case WIFI_FRAME_REASSOC_REQ:
            //
            break;
        case WIFI_FRAME_REASSOC_RESP:
            //
            break;
        case WIFI_FRAME_ATIMS:
            //
            break;
        case WIFI_FRAME_AUTH:
            //
            break;
        case WIFI_FRAME_ACTION:
            //
            break;
        case WIFI_FRAME_RTS:
            result = parse_rts(payload, data->rx_ctrl);
            break;
        case WIFI_FRAME_CTS:
            result = parse_cts(payload, data->rx_ctrl);
            break;
        case WIFI_FRAME_DATA:
        case WIFI_FRAME_DATA_ALT:
            result = parse_data(payload, data->rx_ctrl);
            break;
        default:
            //
            break;
    }
    if (result != ESP_OK) {
        // TODO: Something?
    }
    return;
}

esp_err_t initialise_wifi() {
    /* Initialise WiFi if needed */
    if (!WIFI_INITIALISED) {
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        esp_netif_create_default_wifi_ap();

        /* Set up promiscuous mode */
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        // NOTE: This was previously WIFI_STORAGE_RAM
        ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));

        /* Dummy AP to specify a channel and get WiFi hardware into a
           mode where we can send arbitrary frames. */
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        wifi_config_t ap_config = {
            .ap = {
                .ssid = "ManagementAP",
                .ssid_len = 12,
                .password = "management",
                .channel = 1,
                .authmode = WIFI_AUTH_OPEN,
                .ssid_hidden = 0,
                .max_connection = 128,
                .beacon_interval = 5000
            }
        };
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
        ESP_ERROR_CHECK(esp_wifi_start());
        ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

        /* Register the frame types we're interested in */
        wifi_promiscuous_filter_t filter = { .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_CTRL | WIFI_PROMIS_FILTER_MASK_DATA };
        esp_wifi_set_promiscuous_filter(&filter);
        esp_wifi_set_promiscuous_rx_cb(wifi_pkt_rcvd);

        WIFI_INITIALISED = true;
    }

    /* Initialise channels[] to include all supported channels */
    if (channels != NULL) {
        free(channels);
        channels_count = 0;
    }
    channels = malloc(WENDIGO_SUPPORTED_24_CHANNELS_COUNT);
    if (channels != NULL) {
        channels_count = WENDIGO_SUPPORTED_24_CHANNELS_COUNT;
        for (uint8_t i = 0; i < channels_count; ++i) {
            channels[i] = WENDIGO_SUPPORTED_24_CHANNELS[i];
        }
    }
    return ESP_OK;
}

/* Enable wifi scanning */
esp_err_t wendigo_wifi_enable() {
    esp_err_t result = ESP_OK;
    if (!WIFI_INITIALISED) {
        result = initialise_wifi();
    }
    result |= esp_wifi_set_promiscuous(true);
    create_hop_task_if_needed();
    return result;
}

/* Disable wifi scanning */
esp_err_t wendigo_wifi_disable() {
    esp_wifi_set_promiscuous(false);
    if (channelHopTask != NULL) {
        if (scanStatus[SCAN_INTERACTIVE] == ACTION_ENABLE) {
            ESP_LOGI(WIFI_TAG, "Killing WiFi channel hopping task %p...", &channelHopTask);
        }
        vTaskDelete(channelHopTask);
        channelHopTask = NULL;
    }
    return ESP_OK;
}

/** Check whether the specified value is a valid WiFi channel */
bool wendigo_is_valid_channel(uint8_t channel) {
    uint8_t channelIdx;
    for (channelIdx = 0; channelIdx < WENDIGO_SUPPORTED_24_CHANNELS_COUNT &&
        WENDIGO_SUPPORTED_24_CHANNELS[channelIdx] != channel; ++channelIdx) { }
    return (channelIdx < WENDIGO_SUPPORTED_24_CHANNELS_COUNT);
}

/** Display the channels that are currently included in channel hopping.
 * In Interactive Mode this displays a readable string, in Flipper mode
 * a comma-separated list of active channels.
 */
esp_err_t wendigo_get_channels() {
    if (scanStatus[SCAN_INTERACTIVE] == ACTION_ENABLE) {
        printf("%d channels included in WiFi channel hopping: ", channels_count);
        for (uint8_t i = 0; i < channels_count; ++i) {
            printf("%s%d", (i > 0) ? ", " : "", channels[i]);
        }
        putchar('\n');
    } else {
        repeat_bytes(0x99, 4);
        repeat_bytes(0xAA, 4);
        send_bytes(&channels_count, sizeof(uint8_t));
        if (channels_count > 0) {
            send_bytes((uint8_t *)channels, sizeof(uint16_t) * channels_count);
        }
        send_end_of_packet();
    }
    return ESP_OK;
}

/** Set the channels that are to be included in channel hopping.
 * channels[] is an array of length channels_count, with each element
 * representing a channel that is to be enabled.
 */
esp_err_t wendigo_set_channels(uint8_t *new_channels, uint8_t new_channels_count) {
    if (channels != NULL) {
        free(channels);
        channels = NULL;
        channels_count = 0;
    }
    if (new_channels_count > 0) {
        channels = malloc(new_channels_count);
        if (channels == NULL) {
            return ESP_ERR_NO_MEM;
        }
        memcpy(channels, new_channels, new_channels_count);
        channels_count = new_channels_count;
    }
    return ESP_OK;
}

void create_hop_task_if_needed() {
    if (channelHopTask == NULL) {
        if (scanStatus[SCAN_INTERACTIVE] == ACTION_ENABLE) {
            ESP_LOGI(WIFI_TAG, "Channel hopping task is not running, starting it now...");
        }
        xTaskCreate(channelHopCallback, "channelHopCallback", 2048, NULL, 5, &channelHopTask);
    }
    /* Restart hopping from beginning of supported channels list */
    channel_index = 0;
}

void channelHopCallback(void *pvParameter) {
    if (hop_millis == 0) {
        hop_millis = CONFIG_DEFAULT_HOP_MILLIS;
    }
    while (true) {
        /* Delay hop_millis ms */
        vTaskDelay(hop_millis / portTICK_PERIOD_MS);
        /* Only hop if there are channels to hop to */
        if (channels_count > 0) {
            ++channel_index; /* Move to next supported channel */
            if (channel_index >= channels_count) {
                /* Current channel is not in active channels or next channel is first channel */
                channel_index = 0;
            }
            if (esp_wifi_set_channel(channels[channel_index], WIFI_SECOND_CHAN_NONE) != ESP_OK &&
                    scanStatus[SCAN_INTERACTIVE] == ACTION_ENABLE) {
                ESP_LOGW(WIFI_TAG, "Failed to change to channel %d", channels[channel_index]);
            }
        }
    }
}
