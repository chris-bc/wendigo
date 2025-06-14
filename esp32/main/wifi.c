#include "wifi.h"
bool WIFI_INITIALISED = false;
/* Override the default implementation so we can send arbitrary 802.11 packets */
esp_err_t ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2, int32_t arg3) {
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
        dev->radio.ap.ssid[0] = '\0';
    }
    dev->scanType = SCAN_WIFI_AP;
    dev->rssi = rx_ctrl.rssi;
    dev->radio.ap.channel = rx_ctrl.channel;
    uint8_t ssid_len = payload[BEACON_SSID_OFFSET - 1];
    memcpy(dev->radio.ap.ssid, payload + BEACON_SSID_OFFSET, ssid_len);
    dev->radio.ap.ssid[ssid_len] = '\0';

    esp_err_t result = add_device(dev);
    if (creating) {
        free(dev);
    }
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

    esp_err_t result = add_device(dev);
    if (creating) {
        free(dev);
    }
    return result;
}

/** Parse a probe response packet, creating or updating a wendigo_device
 * for both the source (AP) and destination (STA).
 */
esp_err_t parse_probe_resp(uint8_t *payload, wifi_pkt_rx_ctrl_t rx_ctrl) {
    wendigo_device *ap = retrieve_by_mac(payload + PROBE_RESPONSE_BSSID_OFFSET);
    wendigo_device *sta = NULL;
    bool creating = false;
    esp_err_t result = ESP_OK;

    if (memcmp(broadcastMac, payload + PROBE_RESPONSE_DESTADDR_OFFSET, MAC_BYTES)) {
        /* Not a broadcast packet - it's being sent to an actual STA */
        sta = retrieve_by_mac(payload + PROBE_RESPONSE_DESTADDR_OFFSET);
        if (sta == NULL) {
            creating = true;
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
            result |= add_device(sta);
            if (creating) {
                free(sta);
                creating = false;
                /* Get a pointer to the actual object so we can set its AP later */
                sta = retrieve_by_mac(payload + PROBE_RESPONSE_DESTADDR_OFFSET);
            }
        }
    }
    if (ap == NULL) {
        creating = true;
        ap = malloc(sizeof(wendigo_device));
        if (ap == NULL) {
            return outOfMemory();
        }
        memcpy(ap->mac, payload + PROBE_RESPONSE_BSSID_OFFSET, MAC_BYTES);
        ap->tagged = false;
        ap->radio.ap.stations = NULL;   /* We'll update these later */
        ap->radio.ap.stations_count = 0;
        ap->radio.ap.ssid[0] = '\0';
    }
    ap->scanType = SCAN_WIFI_AP;
    ap->rssi = rx_ctrl.rssi;
    ap->radio.ap.channel = rx_ctrl.channel;
    uint8_t ssid_len = payload[PROBE_RESPONSE_SSID_OFFSET - 1];
    if (ssid_len > 0) {
        memcpy(ap->radio.ap.ssid, payload + PROBE_RESPONSE_SSID_OFFSET, ssid_len);
    }
    ap->radio.ap.ssid[ssid_len] = '\0';
    result |= add_device(ap);
    if (creating) {
        free(ap);
        ap = retrieve_by_mac(payload + PROBE_RESPONSE_BSSID_OFFSET);
    }
    /* Even though this is a probe response, meaning the STA and AP have not successfully
       connected, link the AP and STA to each other because we don't currently parse enough
       packets to set the links upon successful association. */
    // TODO: Add parsing for data & association packets
    if (ap != NULL && sta != NULL) {
        sta->radio.sta.ap = ap;
        memcpy(sta->radio.sta.apMac, ap->mac, MAC_BYTES);
        wendigo_device **new_stations = realloc(ap->radio.ap.stations, sizeof(wendigo_device *) * (ap->radio.ap.stations_count + 1));
        if (new_stations != NULL) {
            ap->radio.ap.stations = (void **)new_stations;
            ap->radio.ap.stations[ap->radio.ap.stations_count++] = sta;
        }
    }
    return result;
}

esp_err_t parse_rts(uint8_t *payload, wifi_pkt_rx_ctrl_t rx_ctrl) {
    //

    return ESP_OK;
}

esp_err_t parse_cts(uint8_t *payload, wifi_pkt_rx_ctrl_t rx_ctrl) {
    //

    return ESP_OK;
}

esp_err_t parse_data(uint8_t *payload, wifi_pkt_rx_ctrl_t rx_ctrl) {
    //

    return ESP_OK;
}

esp_err_t parse_deauth(uint8_t *payload, wifi_pkt_rx_ctrl_t rx_ctrl) {
    //

    return ESP_OK;
}

esp_err_t parse_disassoc(uint8_t *payload, wifi_pkt_rx_ctrl_t rx_ctrl) {
    //

    return ESP_OK;
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
    return ESP_OK;
}

/* Enable wifi scanning */
esp_err_t wendigo_wifi_enable() {
    esp_err_t result = ESP_OK;
    if (!WIFI_INITIALISED) {
        result = initialise_wifi();
    }
    result |= esp_wifi_set_promiscuous(true);
    return result;
}

/* Disable wifi scanning */
esp_err_t wendigo_wifi_disable() {
    esp_wifi_set_promiscuous(false);
    return ESP_OK;
}