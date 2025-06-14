#include "wifi.h"
bool WIFI_INITIALISED = false;
/* Override the default implementation so we can send arbitrary 802.11 packets */
esp_err_t ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2, int32_t arg3) {
    return ESP_OK;
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
        ap->radio.ap.stations = (void **)new_stations;
        ap->radio.ap.stations[ap->radio.ap.stations_count++] = sta;
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
        dev->radio.ap.ssid[0] = '\0';
    }
    dev->scanType = SCAN_WIFI_AP;
    dev->rssi = rx_ctrl.rssi;
    dev->radio.ap.channel = rx_ctrl.channel;
    uint8_t ssid_len = payload[BEACON_SSID_OFFSET - 1];
    memcpy(dev->radio.ap.ssid, payload + BEACON_SSID_OFFSET, ssid_len);
    dev->radio.ap.ssid[ssid_len] = '\0';

    esp_err_t result = ESP_OK;
    if (creating) {
        result = add_device(dev);
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

    esp_err_t result = ESP_OK;
    if (creating) {
        result = add_device(dev);
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
            /* Create a wendigo_device for STA */
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
            if (creating) {
                result |= add_device(sta);
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
    if (creating) {
        result |= add_device(ap);
        free(ap);
        ap = retrieve_by_mac(payload + PROBE_RESPONSE_BSSID_OFFSET);
    }
    /* Even though this is a probe response, meaning the STA and AP have not successfully
       connected, link the AP and STA to each other because we don't currently parse enough
       packets to set the links upon successful association. */
    if (ap != NULL && sta != NULL) {
        result |= set_associated(sta, ap);
    }
    return result;
}

/** Parse an RTS (request to send) frame and create or update the
 * wendigo_device representing the transmitting STA and receiving AP.
 */
esp_err_t parse_rts(uint8_t *payload, wifi_pkt_rx_ctrl_t rx_ctrl) {
    wendigo_device *sta = retrieve_by_mac(payload + RTS_CTS_SRCADDR);
    wendigo_device *ap = retrieve_by_mac(payload + RTS_CTS_DESTADDR);
    bool creating = false;
    esp_err_t result = ESP_OK;
    if (sta == NULL) {
        creating = true;
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
    if (creating) {
        result |= add_device(sta);
        free(sta);
        creating = false;
        sta = retrieve_by_mac(payload + RTS_CTS_SRCADDR);
    }
    if (ap == NULL) {
        /* Even though the only AP info we have is MAC, we also know
           that STA is connected to it - Create a wendigo_device for
           the AP so the connection can be recorded. */
           creating = true;
           ap = malloc(sizeof(wendigo_device));
           if (ap == NULL) {
            return outOfMemory();
           }
           memcpy(ap->mac, payload + RTS_CTS_DESTADDR, MAC_BYTES);
           ap->tagged = false;
           ap->radio.ap.ssid[0] = '\0';
           ap->radio.ap.stations = NULL;
           ap->radio.ap.stations_count = 0;
    }
    ap->scanType = SCAN_WIFI_AP;
    ap->radio.ap.channel = rx_ctrl.channel;
    if (creating) {
        result |= add_device(ap);
        free(ap);
        ap = retrieve_by_mac(payload + RTS_CTS_DESTADDR);
    }
    /* Link the AP and STA */
    if (ap != NULL && sta != NULL) {
        result |= set_associated(sta, ap);
    }
    return result;
}

/** Parse a CTS (clear to send) packet and create or update the wendigo_device
 * objects representing the AP and STA.
 */
esp_err_t parse_cts(uint8_t *payload, wifi_pkt_rx_ctrl_t rx_ctrl) {
    wendigo_device *sta = retrieve_by_mac(payload + RTS_CTS_DESTADDR);
    wendigo_device *ap = retrieve_by_mac(payload + RTS_CTS_SRCADDR);
    bool creating = false;
    esp_err_t result = ESP_OK;

    if (ap == NULL) {
        /* Create the AP */
        creating = true;
        ap = malloc(sizeof(wendigo_device));
        if (ap == NULL) {
            return outOfMemory();
        }
        memcpy(ap->mac, payload + RTS_CTS_SRCADDR, MAC_BYTES);
        ap->tagged = false;
        ap->radio.ap.ssid[0] = '\0';
        ap->radio.ap.stations_count = 0;
        ap->radio.ap.stations = NULL;
    }
    ap->scanType = SCAN_WIFI_AP;
    ap->rssi = rx_ctrl.rssi;
    ap->radio.ap.channel = rx_ctrl.channel;
    if (creating) {
        result |= add_device(ap);
        free(ap);
        creating = false;
        ap = retrieve_by_mac(payload + RTS_CTS_SRCADDR);
    }
    if (sta == NULL) {
        /* While the only thing we know about the STA is its MAC, create
           a wendigo_device for it so it can be linked to the AP. */
        creating = true;
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
    if (creating) {
        result |= add_device(sta);
        free(sta);
        sta = retrieve_by_mac(payload + RTS_CTS_DESTADDR);
    }
    /* Link the AP and STA */
    if (ap != NULL && sta != NULL) {
        set_associated(sta, ap);
    }
    return result;
}

/** Parse a data packet. These could go in either direction so we know the source
 * and destination MAC, but don't know which is an AP and which a STA. This function
 * will search for both MACs in the device cache to determine which is which. If
 * both devices are present in the cache they are linked.
 */
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