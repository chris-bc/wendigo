#include "wifi.h"
bool WIFI_INITIALISED = false;
/* Override the default implementation so we can send arbitrary 802.11 packets */
esp_err_t ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2, int32_t arg3) {
    return ESP_OK;
}

esp_err_t parse_beacon(uint8_t *payload, wifi_pkt_rx_ctrl_t rx_ctrl) {
    //

    return ESP_OK;
}

esp_err_t parse_probe_req(uint8_t *payload, wifi_pkt_rx_ctrl_t rx_ctrl) {
    //

    return ESP_OK;
}

esp_err_t parse_probe_resp(uint8_t *payload, wifi_pkt_rx_ctrl_t rx_ctrl) {
    //

    return ESP_OK;
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
            //
            break;
        case WIFI_FRAME_DISASSOC:
            //
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