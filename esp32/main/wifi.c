#include "wifi.h"
#include "common.h"
#include "esp_err.h"
#include "freertos/idf_additions.h"
#include "portmacro.h"

/* Array of channels that are to be included in channel hopping.
   At startup this is initialised to include all supported channels. */
uint8_t *channels = NULL;
uint8_t channels_count = 0;
uint8_t channel_index = 0;
// TODO: Refactor to support 5GHz channels if the device supports 5GHz channels
const uint8_t WENDIGO_SUPPORTED_24_CHANNELS_COUNT = 14;
const uint8_t WENDIGO_SUPPORTED_24_CHANNELS[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
const uint8_t WENDIGO_SUPPORTED_5_CHANNELS_COUNT = 31;
const uint8_t WENDIGO_SUPPORTED_5_CHANNELS[] = {32, 36, 40, 44, 48, 52, 56, 60, 64, 68, 96, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144, 149, 153, 157, 161, 165, 169, 173, 177};
const uint8_t PRIVACY_ON_BITS[] = {0x11, 0x11};
const uint8_t PRIVACY_OFF_BITS[] = {0x01, 0x11};
long hop_millis = CONFIG_DEFAULT_HOP_MILLIS;
TaskHandle_t channelHopTask = NULL; /* Independent task for channel hopping */

// TODO: This is duplicated for Flipper-Wendigo because the ifndef guard isn't working
uint8_t auth_mode_strings_count = 17;
char *wifi_auth_mode_strings[] = {"Open", "WEP", "WPA", "WPA2",
    "WPA+WPA2", "EAP", "EAP", "WPA3", "WPA2+WPA3", "WAPI", "OWE",
    "WPA3 Enterprise 192-bit", "WPA3 EXT", "WPA3 EXT Mixed Mode", "DPP",
    "WPA3 Enterprise", "WPA3 Enterprise Transition", "Unknown"};

bool WIFI_INITIALISED = false;
uint8_t BANNER_WIDTH = 62;
static const char *WIFI_TAG = "WiFi@Wendigo";

/* Local function declarations */
void create_hop_task_if_needed();
void channelHopCallback(void *pvParameter);

/** Override the default implementation so we can send arbitrary 802.11 packets */
esp_err_t ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2, int32_t arg3) {
    return ESP_OK;
}

esp_err_t display_wifi_ap_uart(wendigo_device *dev) {
    esp_err_t result = ESP_OK;
    if (dev->scanType != SCAN_WIFI_AP) {
        return ESP_ERR_INVALID_ARG;
    }
    /* Send dev->tagged as 1 for true, 0 for false */
    uint8_t tagged = (dev->tagged) ? 1 : 0;
    /* Calculate ssid_len */
    uint8_t ssid_len = strnlen((char *)dev->radio.ap.ssid, MAX_SSID_LEN + 1);
    if (dev->radio.ap.ssid[0] == '\0') {
        ssid_len = 0;
    }
    /* Assemble the packet */
    uint8_t packet_len = WENDIGO_OFFSET_AP_SSID + ssid_len + (MAC_BYTES * dev->radio.ap.stations_count) + PREAMBLE_LEN;
    uint8_t *packet = malloc(sizeof(uint8_t) * packet_len);
    if (packet == NULL) {
        return outOfMemory();
    }
    memcpy(packet, PREAMBLE_WIFI_AP, PREAMBLE_LEN);
    memcpy(packet + WENDIGO_OFFSET_WIFI_SCANTYPE, &(dev->scanType), sizeof(uint8_t));
    memcpy(packet + WENDIGO_OFFSET_WIFI_MAC, dev->mac, MAC_BYTES);
    memcpy(packet + WENDIGO_OFFSET_WIFI_CHANNEL, &(dev->radio.ap.channel), sizeof(uint8_t));
    memcpy(packet + WENDIGO_OFFSET_WIFI_RSSI, (uint8_t *)&(dev->rssi), sizeof(int16_t));
    /* Don't bother sending lastSeen */
    //memcpy(packet + WENDIGO_OFFSET_WIFI_LASTSEEN, (uint8_t *)&(dev->lastSeen.tv_sec), sizeof(int64_t));
    memcpy(packet + WENDIGO_OFFSET_WIFI_TAGGED, &tagged, sizeof(uint8_t));
    memcpy(packet + WENDIGO_OFFSET_AP_AUTH_MODE, &(dev->radio.ap.authmode), sizeof(uint8_t));
    memcpy(packet + WENDIGO_OFFSET_AP_SSID_LEN, &ssid_len, sizeof(uint8_t));
    memcpy(packet + WENDIGO_OFFSET_AP_STA_COUNT, &(dev->radio.ap.stations_count), sizeof(uint8_t));
    if (ssid_len > 0) {
        memcpy(packet + WENDIGO_OFFSET_AP_SSID, dev->radio.ap.ssid, ssid_len);
    }
    /* Keep track of the offset while working through stations[] */
    uint8_t current_offset = WENDIGO_OFFSET_AP_SSID + ssid_len;
    for (uint8_t i = 0; i < dev->radio.ap.stations_count; ++i) {
        /* Send the MAC of each connected device */
        /* It should be impossible to get a NULL station, but cater for it anyway */
        if (dev->radio.ap.stations == NULL || dev->radio.ap.stations[i] == NULL) {
            memcpy(packet + current_offset, nullMac, MAC_BYTES);
        } else {
            memcpy(packet + current_offset, dev->radio.ap.stations[i], MAC_BYTES);
        }
        current_offset += MAC_BYTES;
    }
    memcpy(packet + current_offset, PACKET_TERM, PREAMBLE_LEN);
    /* Send the packet */
    if (xSemaphoreTake(uartMutex, portMAX_DELAY)) {
        send_bytes(packet, packet_len);
        xSemaphoreGive(uartMutex);
    } else {
        result = ESP_ERR_INVALID_STATE;
    }
    free(packet);
    return result;
}

esp_err_t display_wifi_ap_interactive(wendigo_device *dev) {
    esp_err_t result = ESP_OK;
    if (dev == NULL || dev->scanType != SCAN_WIFI_AP) {
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
        space_left = (BANNER_WIDTH - MAC_STRLEN - 39) / 2;
        printf("(%s)", macStr);
        print_space(space_left, false);
        printf("Ch. %2d", dev->radio.ap.channel); // TODO: Make space for a additional character, for 5GHz channels
        /* Cater for rounding in space_left */
        row_len = MAC_STRLEN + 40 + space_left;
        print_space(BANNER_WIDTH - row_len, false);
        printf("%3d Stations Connected", dev->radio.ap.stations_count);
        print_space(4, false);
        print_star(1, true);
    }
    // TODO: Integrate authmode in the above
    print_star(1, false);
    print_space(4, false);
    if (dev->radio.ap.authmode > auth_mode_strings_count) {
        dev->radio.ap.authmode = auth_mode_strings_count;
    }
    printf("Authentication: %s", wifi_auth_mode_strings[dev->radio.ap.authmode]);
    // BANNER_WIDTH - 10 - 16 - strlen() + 4 spaces
    print_space(BANNER_WIDTH - 22 - strlen(wifi_auth_mode_strings[dev->radio.ap.authmode]), false);
    print_star(1, true);
    print_star(BANNER_WIDTH, true);
    return result;
}

esp_err_t display_wifi_sta_uart(wendigo_device *dev) {
    esp_err_t result = ESP_OK;
    if (dev->scanType != SCAN_WIFI_STA) {
        return ESP_ERR_INVALID_ARG;
    }
    /* Send tagged as 1 for true, 0 for false */
    uint8_t tagged = (dev->tagged) ? 1 : 0;
    /* Calculate ssid_len */
    uint8_t ssid_len = 0;
    char *ssid = NULL;
    wendigo_device *theAP = retrieve_by_mac(dev->radio.sta.apMac);
    if (theAP != NULL && theAP->radio.ap.ssid[0] != '\0') {
        ssid = theAP->radio.ap.ssid;
        ssid_len = strlen(ssid);
    }
    /* Assemble the packet so it can be sent all at once */
    uint8_t packet_len = WENDIGO_OFFSET_STA_AP_SSID + ssid_len + PREAMBLE_LEN;
    /* Also account for the preferred network list, if it exists */
    for (uint8_t i = 0; i < dev->radio.sta.saved_networks_count; ++i) {
        if (dev->radio.sta.saved_networks[i] != NULL) {
            packet_len += strlen(dev->radio.sta.saved_networks[i]);
        }
        /* Account for the SSID_LEN byte - whether or not current SSID is NULL */
        ++packet_len;
    }
    uint8_t *packet = malloc(sizeof(uint8_t) * packet_len);
    if (packet == NULL) {
        return outOfMemory();
    }
    memcpy(packet, PREAMBLE_WIFI_STA, PREAMBLE_LEN);
    memcpy(packet + WENDIGO_OFFSET_WIFI_SCANTYPE, &(dev->scanType), sizeof(uint8_t));
    memcpy(packet + WENDIGO_OFFSET_WIFI_MAC, dev->mac, MAC_BYTES);
    memcpy(packet + WENDIGO_OFFSET_WIFI_CHANNEL, &(dev->radio.sta.channel), sizeof(uint8_t));
    memcpy(packet + WENDIGO_OFFSET_WIFI_RSSI, (uint8_t *)&(dev->rssi), sizeof(int16_t));
    /* Don't bother sending lastSeen */
    //memcpy(packet + WENDIGO_OFFSET_WIFI_LASTSEEN, (uint8_t *)&(dev->lastSeen.tv_sec), sizeof(int64_t));
    memcpy(packet + WENDIGO_OFFSET_WIFI_TAGGED, &tagged, sizeof(uint8_t));
    memcpy(packet + WENDIGO_OFFSET_STA_AP_MAC, dev->radio.sta.apMac, MAC_BYTES);
    memcpy(packet + WENDIGO_OFFSET_STA_AP_SSID_LEN, &ssid_len, sizeof(uint8_t));
    /* Send SSID if we have one */
    if (ssid_len > 0) {
        memcpy(packet + WENDIGO_OFFSET_STA_AP_SSID, ssid, ssid_len);
    }
    /* Send saved_networks_count */
    memcpy(packet + WENDIGO_OFFSET_STA_PNL_COUNT, &(dev->radio.sta.saved_networks_count), sizeof(uint8_t));
    /* Send saved_networks[] */
    uint8_t packetIdx = WENDIGO_OFFSET_STA_AP_SSID + ssid_len;
    uint8_t pnl_len;
    for (uint8_t i = 0; i < dev->radio.sta.saved_networks_count; ++i) {
        if (dev->radio.sta.saved_networks[i] == NULL) {
            pnl_len = 0;
        } else {
            pnl_len = strlen(dev->radio.sta.saved_networks[i]);
        }
        /* Add current SSID len */
        memcpy(packet + packetIdx, &pnl_len, sizeof(uint8_t));
        ++packetIdx;
        /* Add current SSID */
        if (pnl_len > 0) {
            memcpy(packet + packetIdx, dev->radio.sta.saved_networks[i], pnl_len);
            packetIdx += pnl_len;
        }
    }
    /* Packet terminator */
    memcpy(packet + packetIdx, PACKET_TERM, PREAMBLE_LEN);
    /* Send the packet */
    if (xSemaphoreTake(uartMutex, portMAX_DELAY)) {
        send_bytes(packet, packet_len);
        xSemaphoreGive(uartMutex);
    } else {
        result = ESP_ERR_INVALID_STATE;
    }
    free(packet);
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
    if (memcmp(dev->radio.sta.apMac, nullMac, MAC_BYTES)) {
        char apMacStr[MAC_STRLEN + 1];
        mac_bytes_to_string(dev->radio.sta.apMac, apMacStr);
        char ssid[MAX_SSID_LEN + 1];
        explicit_bzero(ssid, MAX_SSID_LEN + 1);
        wendigo_device *theAP = retrieve_by_mac(dev->radio.sta.apMac);
        if (theAP != NULL) {
            strncpy(ssid, theAP->radio.ap.ssid, MAX_SSID_LEN + 1);
            ssid[MAX_SSID_LEN] = '\0';  /* Just in case */
        }
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
    /* Display saved networks */
    print_star(1, false);
    print_space(4, false);
    printf("%3d saved networks collected.", dev->radio.sta.saved_networks_count);
    print_space(BANNER_WIDTH - 35, false);
    print_star(1, true);
    if (dev->radio.sta.saved_networks_count > 0) {
        /* Display each saved network */
        uint8_t ssid_len;
        for (uint8_t i = 0; i < dev->radio.sta.saved_networks_count; ++i) {
            if (dev->radio.sta.saved_networks[i] != NULL) {
                ssid_len = strlen(dev->radio.sta.saved_networks[i]);
                print_star(1, false);
                print_space(8, false);
                printf("* %s", dev->radio.sta.saved_networks[i]);
                print_space(BANNER_WIDTH - ssid_len - 12, false);
                print_star(1, true);
            }
        }
    }
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
    memcpy(sta->radio.sta.apMac, ap->mac, MAC_BYTES);
    /* See if sta's MAC is present in ap->radio.ap.stations */
    uint8_t i;
    for (i = 0; i < ap->radio.ap.stations_count && (ap->radio.ap.stations[i] == NULL || memcmp(sta->mac, ap->radio.ap.stations[i], MAC_BYTES)); ++i) { }
    if (i == ap->radio.ap.stations_count) {
        /* Station not found in ap->radio.ap.stations - Add it */
        uint8_t **new_stations = realloc(ap->radio.ap.stations, sizeof(uint8_t *) * (ap->radio.ap.stations_count + 1));
        if (new_stations == NULL) {
            return outOfMemory();
        }
        ap->radio.ap.stations = new_stations;
        new_stations[ap->radio.ap.stations_count] = malloc(MAC_BYTES);
        /* Make sure we still increment stations_count if we can't malloc the MAC */
        if (new_stations[ap->radio.ap.stations_count++] == NULL) {
            return outOfMemory();
        }
        memcpy(new_stations[ap->radio.ap.stations_count - 1], sta->mac, MAC_BYTES);
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
        explicit_bzero(dev->radio.ap.ssid, MAX_SSID_LEN + 1);
    }
    dev->scanType = SCAN_WIFI_AP;
    dev->rssi = rx_ctrl.rssi;
    dev->radio.ap.channel = rx_ctrl.channel;
    if (dev->radio.ap.authmode == WIFI_AUTH_MAX) {
        /* Try & get better security info */
        uint8_t privacy;
        memcpy(&privacy, payload + BEACON_PRIVACY_OFFSET, sizeof(uint8_t));
        if (privacy == 0x31) {
            /* It's a protected network. Call it WPA *shrugs* */
            dev->radio.ap.authmode = WIFI_AUTH_WPA_PSK;
        } else if (privacy == 0x21) {
            dev->radio.ap.authmode = WIFI_AUTH_OPEN;
        } else {
            dev->radio.ap.authmode = WIFI_AUTH_MAX;
        }
    }
    uint8_t ssid_len = payload[BEACON_SSID_OFFSET - 1];
    if (ssid_len > 0) {
        memcpy(dev->radio.ap.ssid, payload + BEACON_SSID_OFFSET, ssid_len);
    }
    esp_err_t result = ESP_OK;
    if (creating) {
        result = add_device(dev);
    } else {
        gettimeofday(&(dev->lastSeen), NULL);
    }
    display_wifi_device(dev, creating);
    if (creating) {
        free(dev);
    }
    return result;
}

/** Parse a probe request frame, creating or updating a wendigo_device for
 * the STA that transmitted it.
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
        dev->radio.sta.saved_networks_count = 0;
        dev->radio.sta.saved_networks = NULL;
        memcpy(dev->radio.sta.apMac, nullMac, MAC_BYTES);
    }
    dev->scanType = SCAN_WIFI_STA;
    dev->rssi = rx_ctrl.rssi;
    dev->radio.sta.channel = rx_ctrl.channel;
    uint8_t ssid_len;
    char *ssid;
    memcpy(&ssid_len, payload + PROBE_SSID_OFFSET - 1, sizeof(uint8_t));
    if (ssid_len > 0) {
        ssid = malloc(sizeof(char) * (ssid_len + 1));
        if (ssid != NULL) {
            memcpy(ssid, payload + PROBE_SSID_OFFSET, ssid_len);
            ssid[ssid_len] = '\0';
            uint8_t ssid_idx = wendigo_index_of_string(ssid,
                dev->radio.sta.saved_networks, dev->radio.sta.saved_networks_count);
            if (ssid_idx == dev->radio.sta.saved_networks_count) {
                /* SSID not in STA's saved networks - Add it */
                char **new_pnl = realloc(dev->radio.sta.saved_networks,
                    sizeof(char *) * (dev->radio.sta.saved_networks_count + 1));
                if (new_pnl != NULL) {
                    new_pnl[dev->radio.sta.saved_networks_count] = malloc(sizeof(char) * (ssid_len + 1));
                    if (new_pnl[dev->radio.sta.saved_networks_count] != NULL) {
                        strncpy(new_pnl[dev->radio.sta.saved_networks_count],
                            ssid, ssid_len);
                        new_pnl[dev->radio.sta.saved_networks_count][ssid_len] = '\0';
                        dev->radio.sta.saved_networks = new_pnl;
                        ++(dev->radio.sta.saved_networks_count);
                    }
                }
            } else {
                free(ssid);
                ssid = NULL;
            }
        }
    }

    esp_err_t result = ESP_OK;
    if (creating) {
        result = add_device(dev);
    } else {
        gettimeofday(&(dev->lastSeen), NULL);
    }
    display_wifi_device(dev, creating);
    if (creating) {
        free_device(dev);
    }
    return result;
}

/** Parse a probe response packet, creating or updating a wendigo_device
 * for both the source (AP) and destination (STA).
 // TODO: Extract authMode and populate ap->radio.ap.authmode
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
                sta->radio.sta.saved_networks_count = 0;
                sta->radio.sta.saved_networks = NULL;
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
        explicit_bzero(ap->radio.ap.ssid, MAX_SSID_LEN + 1);
    }
    ap->scanType = SCAN_WIFI_AP;
    ap->rssi = rx_ctrl.rssi;
    ap->radio.ap.channel = rx_ctrl.channel;
    uint8_t ssid_len = payload[PROBE_RESPONSE_SSID_OFFSET - 1];
    if (ssid_len > 0) {
        memcpy(ap->radio.ap.ssid, payload + PROBE_RESPONSE_SSID_OFFSET, ssid_len);
    }
    /* Authentication mode */
    memcpy(&(ap->radio.ap.authmode), payload + ssid_len + PROBE_RESPONSE_AUTH_TYPE_OFFSET, sizeof(uint8_t));
    
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
        sta->radio.sta.saved_networks_count = 0;
        sta->radio.sta.saved_networks = NULL;
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
           explicit_bzero(ap->radio.ap.ssid, MAX_SSID_LEN + 1);
           ap->radio.ap.stations = NULL;
           ap->radio.ap.stations_count = 0;
           ap->radio.ap.authmode = WIFI_AUTH_MAX;
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
        explicit_bzero(ap->radio.ap.ssid, MAX_SSID_LEN + 1);
        ap->radio.ap.stations_count = 0;
        ap->radio.ap.stations = NULL;
        ap->radio.ap.authmode = WIFI_AUTH_MAX;
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
        sta->radio.sta.saved_networks_count = 0;
        sta->radio.sta.saved_networks = NULL;
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
                set_associated(dest, src); // TODO: Call this nyway so that associated MACs are recorded even if we don't have their objects
                                                    // TODO: Check whether data packets can be sent between stations
            }
        } else if (src->scanType == SCAN_WIFI_STA) {
            src->radio.sta.channel = rx_ctrl.channel;
            if (dest != NULL && dest->scanType == SCAN_WIFI_AP) {
                set_associated(src, dest); // TODO: Call anyway as above
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
        explicit_bzero(ap->radio.ap.ssid, MAX_SSID_LEN + 1);
        ap->radio.ap.stations = NULL;
        ap->radio.ap.stations_count = 0;
        ap->radio.ap.authmode = WIFI_AUTH_MAX;
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
        sta->radio.sta.saved_networks_count = 0;
        sta->radio.sta.saved_networks = NULL;
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
        sta->radio.sta.saved_networks_count = 0;
        sta->radio.sta.saved_networks = NULL;
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
        explicit_bzero(ap->radio.ap.ssid, MAX_SSID_LEN + 1);
        ap->radio.ap.stations = NULL;
        ap->radio.ap.stations_count = 0;
        ap->radio.ap.authmode = WIFI_AUTH_MAX;
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

/** Monitor mode callback
 *  This is the callback function invoked when the wireless interface receives any selected packet.
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
            // TODO
            break;
        case WIFI_FRAME_ASSOC_RESP:
            // TODO
            break;
        case WIFI_FRAME_REASSOC_REQ:
            // TODO
            break;
        case WIFI_FRAME_REASSOC_RESP:
            // TODO
            break;
        case WIFI_FRAME_ATIMS:
            // TODO
            break;
        case WIFI_FRAME_AUTH:
            // TODO
            break;
        case WIFI_FRAME_ACTION:
            // TODO
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
                .ssid = "Wendigo WiFi",
                .ssid_len = 12,
                .password = "mythology",
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
    return ESP_OK;
}

/** Enable wifi scanning */
esp_err_t wendigo_wifi_enable() {
    esp_err_t result = ESP_OK;
    if (!WIFI_INITIALISED) {
        result = initialise_wifi();
    }
    /* Set default channels to hop through (all 2.4GHz channels) if not yet configured */
    if (channels == NULL || channels_count == 0) {
        /* Cast the values to avoid compiler warnings about discarding const qualifiers */
        wendigo_set_channels((uint8_t *)WENDIGO_SUPPORTED_24_CHANNELS, (uint8_t)WENDIGO_SUPPORTED_24_CHANNELS_COUNT);
    }
    result |= esp_wifi_set_promiscuous(true);
    create_hop_task_if_needed();
    return result;
}

/** Disable wifi scanning */
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
 * the packet consists of <Preamble><Channel Count><Channel bytes><Terminator>.
 */
esp_err_t wendigo_get_channels() {
    esp_err_t result = ESP_OK;
    if (scanStatus[SCAN_INTERACTIVE] == ACTION_ENABLE) {
        printf("%d channels included in WiFi channel hopping: ", channels_count);
        for (uint8_t i = 0; i < channels_count; ++i) {
            printf("%s%d", (i > 0) ? ", " : "", channels[i]);
        }
        putchar('\n');
    } else {
        /* Assemble the packet with comma-separated channels.
           Packet size is 2*PREAMBLE_LEN + channels_count + 1
        */
        uint8_t packetLen = (2 * PREAMBLE_LEN) + channels_count + 1;
        uint8_t *packet = malloc(packetLen);
        if (packet == NULL) {
            return outOfMemory();
        }
        memcpy(packet, PREAMBLE_CHANNELS, PREAMBLE_LEN);
        memcpy(packet + WENDIGO_OFFSET_CHANNEL_COUNT, &channels_count, sizeof(uint8_t));
        if (channels_count > 0) {
            memcpy(packet + WENDIGO_OFFSET_CHANNELS, channels, channels_count);
        }
        memcpy(packet + WENDIGO_OFFSET_CHANNELS + channels_count, PACKET_TERM, PREAMBLE_LEN);
        /* Transmit the packet */
        if (xSemaphoreTake(uartMutex, portMAX_DELAY)) {
            send_bytes(packet, packetLen);
            xSemaphoreGive(uartMutex);
        } else {
            result = ESP_ERR_INVALID_STATE;
        }
        free(packet);
    }
    return result;
}

/** Set the channels that are to be included in channel hopping.
 * channels[] is an array of length channels_count, with each
 * uint8_t element representing a channel that is to be enabled.
 */
esp_err_t wendigo_set_channels(uint8_t *new_channels, uint8_t new_channels_count) {
    if (channels != NULL && channels_count > 0) {
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

/** Creates and starts a background task to periodically change
 *  WiFi channels if it doesn't already exist.
 *  Regardless of whether or not the task already exists this will
 *  "reset" channel hopping, such that the next channel will be the
 *  first channel in channels[].
 */
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

/** Callback function executed by the channel hopping task once it
 *  has been successfully initialised.
 *  This function enters an infinite loop where it pauses for `hop_millis`
 *  milliseconds and then sets the WiFi channel to the next channel in
 *  channels[].
 */
void channelHopCallback(void *pvParameter) {
    if (hop_millis == 0) {
        /* If Default dwell time for channel hopping is not configured
           default to 500ms (half a second).
           This option is set by running "idf.py menuconfig" from the
           ESP32 project directory and navigating to 
           "Wendigo Configuration" -> "Default dwell time for WiFi when
           channel hopping (milliseconds)".
        */
        hop_millis = (CONFIG_DEFAULT_HOP_MILLIS == 0) ? 500 : CONFIG_DEFAULT_HOP_MILLIS;
    }
    while (true) {
        /* Delay hop_millis ms */
        vTaskDelay(hop_millis / portTICK_PERIOD_MS);
        /* Only hop if there are channels to hop to */
        if (channels_count > 0) {
            ++channel_index; /* Move to next supported channel */
            if (channel_index >= channels_count) {
                /* We've hopped to the end, go back to the start */
                channel_index = 0;
            }
            if (esp_wifi_set_channel(channels[channel_index], WIFI_SECOND_CHAN_NONE) != ESP_OK &&
                    scanStatus[SCAN_INTERACTIVE] == ACTION_ENABLE) {
                ESP_LOGW(WIFI_TAG, "Failed to change to channel %d", channels[channel_index]);
            }
        }
    }
}
