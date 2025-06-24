#include "bluetooth.h"
#include "uuids.c"

#define REMOTE_SERVICE_UUID     0x00FF
#define REMOTE_NOTIFY_CHAR_UUID 0xFF01
#define PROFILE_NUM             1

static void gattc_profile_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
static void ble_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
static void ble_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
esp_err_t cod2deviceStr(uint32_t cod, char *string, uint8_t *stringLen);
esp_err_t cod2shortStr(uint32_t cod, char *string, uint8_t *stringLen);
static bool get_string_name_from_eir(uint8_t *eir, char *bdname, uint8_t *bdname_len);

enum bt_device_parameters {
    BT_PARAM_COD = 0,
    BT_PARAM_RSSI,
    BT_PARAM_EIR,
    BT_PARAM_BDNAME,
    BT_PARAM_COUNT
};

static esp_ble_scan_params_t ble_scan_params = {
    .scan_type          = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval      = 0x50,
    .scan_window        = 0x30,
    .scan_duplicate     = BLE_SCAN_DUPLICATE_DISABLE
};

struct gattc_profile_inst {
    esp_gattc_cb_t gattc_cb;
    uint16_t gattc_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_start_handle;
    uint16_t service_end_handle;
    uint16_t char_handle;
    esp_bd_addr_t remote_bda;
};

/* One gatt-based profile, one app_id and one gattc_if. This array will store the
   gattc_if returned by ESP_GATTS_REG_EVT */
static struct gattc_profile_inst gl_profile_tab[1] = {
    [0] = {
        .gattc_cb = gattc_profile_event_handler,
        .gattc_if = ESP_GATT_IF_NONE,
    },
};

/* Bluetooth attributes used by gattc_profile_event_handler */
static esp_bt_uuid_t remote_filter_service_uuid = {
    .len    = ESP_UUID_LEN_16,
    .uuid   = { .uuid16 = 0x00FF, },
};

static esp_bt_uuid_t remote_filter_char_uuid = {
    .len    = ESP_UUID_LEN_16,
    .uuid   = { .uuid16 = 0xFF01, },
};

static esp_bt_uuid_t notify_descr_uuid = {
    .len    = ESP_UUID_LEN_16,
    .uuid   = { .uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG, },
};
static bool connect     = false;
static bool get_server  = false;
static esp_gattc_char_elem_t *char_elem_result      = NULL;
static esp_gattc_descr_elem_t *descr_elem_result    = NULL;

bool BT_INITIALISED = false;
bool BLE_INITIALISED = false;

esp_err_t display_gap_interactive(wendigo_device *dev) {
    esp_err_t result = ESP_OK;
    /* Before doing anything else check if we have seen this device
       within CONFIG_DELAY_AFTER_DEVICE_DISPLAYED ms */
    if (CONFIG_DELAY_AFTER_DEVICE_DISPLAYED > 0) {
        /* Only display the device if it hasn't been seen before or
           CONFIG_DELAY_AFTER_DEVICE_DISPLAYED ms have passed */
        wendigo_device *existingDevice = retrieve_device(dev);
        if (existingDevice != NULL) {
            /* We've seen it before - decide whether it was too recent and update lastSeen */
            struct timeval nowTime;
            gettimeofday(&nowTime, NULL);
            double elapsed = (nowTime.tv_sec - existingDevice->lastSeen.tv_sec);
            if (elapsed < CONFIG_DELAY_AFTER_DEVICE_DISPLAYED) {
                /* It's been seen too recently, don't display it */
                return ESP_OK;
            }
        }
    }
    bool cod_fit = false;
    char *dev_type = (dev->scanType == SCAN_HCI) ? radioShortNames[SCAN_HCI] :
                      (dev->scanType == SCAN_BLE) ? radioShortNames[SCAN_BLE] : "UNK";
    char mac_str[MAC_STRLEN + 1];
    mac_bytes_to_string(dev->mac, mac_str);
    char cod_str[COD_MAX_LEN];
    uint8_t cod_str_len = 0;
    cod2deviceStr(dev->radio.bluetooth.cod, cod_str, &cod_str_len);

    const int banner_width = 62;
    print_star(banner_width, true);
    print_star(1, false);
    print_space(4, false);
    printf("%3s Device %s", dev_type, mac_str);
    print_space(14, false);
    printf("RSSI: %4d", dev->rssi);
    print_space(4, false);
    print_star(1, true);
    /* Put name on line 2 if we have a name */
    if (dev->radio.bluetooth.bdname_len > 0 && dev->radio.bluetooth.bdname != NULL) {
        print_star(1, false);
        print_space(4, false);
        printf("Name: \"%s\"", dev->radio.bluetooth.bdname);
        /* Can name and COD fit on a single line? */
        /* Requires 10 + "Name: "bdname" Class: COD" < 62 => 26 + strlen(bdname) + strlen(cod) <= 62 */
        if ((26 + strlen(dev->radio.bluetooth.bdname) + strlen(cod_str)) <= banner_width) {
            cod_fit = true;
            /* How many spaces between name and COD? (62 - 25 (=37) - strlen(bdname) - strlen(cod)) */
            print_space(37 - strlen(dev->radio.bluetooth.bdname) - strlen(cod_str), false);
            printf("Class: %s", cod_str);
            print_space(4, false);
            print_star(1, true);
        } else {
            print_space(banner_width - 14 - strlen(dev->radio.bluetooth.bdname), false);
            print_star(1, true);
        }
    }
    if (!cod_fit) {
        /* COD can be anywhere from 3 characters to 58, we'd better centre it in the banner */
        int num_spaces = ((banner_width - strlen(cod_str)) / 2) - 1;
        print_star(1, false);
        print_space(num_spaces, false);
        printf("%s", cod_str);
        print_space(banner_width - 2 - num_spaces - strlen(cod_str), false);
        print_star(1, true);
    }
    print_star(banner_width, true);

    return result;
}

/* Send device info to stdout to transmit over UART
   * Sends 4 bytes of 0xFF followed by 4 bytes of 0xAA to begin the transmission
   * Sends device attributes
   * Ends the transmission with the reverse of the start: 4*0xAA 4*0xFF
*/
esp_err_t display_gap_uart(wendigo_device *dev) {
    esp_err_t result = ESP_OK;
    char cod_short[SHORT_COD_MAX_LEN];
    uint8_t cod_len;
    cod2shortStr(dev->radio.bluetooth.cod, cod_short, &cod_len);
    /* Account for NULL terminator on cod_short */
    ++cod_len;
    /* Send RSSI as a char[4] to avoid casting between uint8_t and int16_t */
    char rssi[RSSI_LEN + 3];    /* +3 rather than 1 to avoid compiler errors - int16_t can be up to 6 chars */
    memset(rssi, '\0', RSSI_LEN + 3);
    snprintf(rssi, RSSI_LEN + 3, "%d", dev->rssi);
    /* Send tagged as 1 for true, 0 for false */
    uint8_t tagged = (dev->tagged) ? 1 : 0;

    /* Assemble the packet and send it in one go - I can't figure out why AP packet length
       is a byte too long, this will at least cover up the issue :/ */
    uint8_t packet_len = WENDIGO_OFFSET_BT_BDNAME + dev->radio.bluetooth.bdname_len +
        dev->radio.bluetooth.eir_len + cod_len + PREAMBLE_LEN;
    uint8_t *packet = malloc(sizeof(uint8_t *) * packet_len);
    if (packet == NULL) {
        return outOfMemory();
    }
    memcpy(packet, PREAMBLE_BT_BLE, PREAMBLE_LEN);
    memcpy(packet + WENDIGO_OFFSET_BT_BDNAME_LEN, &(dev->radio.bluetooth.bdname_len), sizeof(uint8_t));
    memcpy(packet + WENDIGO_OFFSET_BT_EIR_LEN, &(dev->radio.bluetooth.eir_len), sizeof(uint8_t));
    memcpy(packet + WENDIGO_OFFSET_BT_RSSI, (uint8_t *)rssi, RSSI_LEN);
    memcpy(packet + WENDIGO_OFFSET_BT_COD, (uint8_t *)&(dev->radio.bluetooth.cod), sizeof(uint32_t));
    memcpy(packet + WENDIGO_OFFSET_BT_BDA, dev->mac, MAC_BYTES);
    memcpy(packet + WENDIGO_OFFSET_BT_SCANTYPE, &(dev->scanType), sizeof(uint8_t));
    memcpy(packet + WENDIGO_OFFSET_BT_TAGGED, &tagged, sizeof(uint8_t));
    memcpy(packet + WENDIGO_OFFSET_BT_LASTSEEN, (uint8_t *)&(dev->lastSeen.tv_sec), sizeof(int64_t));
    memcpy(packet + WENDIGO_OFFSET_BT_NUM_SERVICES,
        &(dev->radio.bluetooth.bt_services.num_services),
        sizeof(uint8_t));
    memcpy(packet + WENDIGO_OFFSET_BT_KNOWN_SERVICES_LEN,
        &(dev->radio.bluetooth.bt_services.known_services_len),
        sizeof(uint8_t));
    memcpy(packet + WENDIGO_OFFSET_BT_COD_LEN, &cod_len, sizeof(uint8_t));

    /* bdname */
    if (dev->radio.bluetooth.bdname_len > 0) {
        memcpy(packet + WENDIGO_OFFSET_BT_BDNAME, (uint8_t *)(dev->radio.bluetooth.bdname),
            dev->radio.bluetooth.bdname_len); /* NOTE: No longer null-terminated */
    }
    /* EIR */
    if (dev->radio.bluetooth.eir_len > 0) {
        memcpy(packet + WENDIGO_OFFSET_BT_BDNAME + dev->radio.bluetooth.bdname_len,
            dev->radio.bluetooth.eir, dev->radio.bluetooth.eir_len);
    }
    if (cod_len > 0) {
        memcpy(packet + WENDIGO_OFFSET_BT_BDNAME + dev->radio.bluetooth.bdname_len +
            dev->radio.bluetooth.eir_len, (uint8_t *)cod_short, cod_len); /* NOTE: No longer null-terminated */
    }
    memcpy(packet + WENDIGO_OFFSET_BT_BDNAME + dev->radio.bluetooth.bdname_len +
        dev->radio.bluetooth.eir_len + cod_len, PACKET_TERM, PREAMBLE_LEN);
    /* Send the packet */
    send_bytes(packet, packet_len);
    free(packet);
    return result;
}

/* Display the specified Bluetooth (Classic or LE) device */
esp_err_t display_gap_device(wendigo_device *dev) {
    /* If we're in Focus Mode only display the device if it's tagged
       => Ignore if Focus Mode and device not selected */
    wendigo_device *existingDevice = retrieve_device(dev);
    if (scanStatus[SCAN_FOCUS] == ACTION_ENABLE && (existingDevice == NULL || !existingDevice->tagged)) {
        return ESP_OK;
    }
    if (scanStatus[SCAN_INTERACTIVE] == ACTION_ENABLE) {
        return display_gap_interactive(dev);
    } else {
        return display_gap_uart(dev);
    }
}

esp_err_t wendigo_bt_initialise() {
    esp_err_t result = ESP_OK;
    if (!BT_INITIALISED) {
        esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
        #if defined(CONFIG_BT_CLASSIC_ENABLED)
            #if defined(CONFIG_BT_BLE_ENABLED)
                bt_cfg.mode = ESP_BT_MODE_BTDM;
            #else
                bt_cfg.mode = ESP_BT_MODE_CLASSIC_BT;
            #endif
        #else
            #if defined(CONFIG_BT_BLE_ENABLED)
                bt_cfg.mode = ESP_BT_MODE_BLE;
            #else
                /* No Bluetooth support */
                bt_cfg.mode = ESP_BT_MODE_IDLE;
                return ESP_ERR_NOT_SUPPORTED;
            #endif
        #endif
        result |= esp_bt_controller_init(&bt_cfg);
        /* Enable WiFi sleep mode in order for wireless coexistence to work */
        esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
        result |= esp_bt_controller_enable(bt_cfg.mode);
        result |= esp_bluedroid_init();
        result |= esp_bluedroid_enable();
        BT_INITIALISED = true;
    }
    return result;
}

esp_err_t wendigo_ble_initialise() {
    esp_err_t result = ESP_OK;
    esp_err_t step = ESP_OK;
    if (!BT_INITIALISED) {
        result = wendigo_bt_initialise();
    }
    /* Not testing the result because BLE can be supported independently of BLE */
    // TODO: This needs to be tested extensively - Bluetooth functions have been created on the assumption
    //       that both BT Classic and BLE are enabled. Test different combinations of BT/BLE support
    if (!BLE_INITIALISED) {
        step = esp_ble_gap_register_callback(ble_gap_cb);
        if (step != ESP_OK && scanStatus[SCAN_INTERACTIVE] == ACTION_ENABLE) {
            ESP_LOGE(BLE_TAG, "Error registering BLE GAP callback: %s", esp_err_to_name(step));
        }
        result |= step;
        
        step = esp_ble_gattc_register_callback(ble_gattc_cb);
        if (step != ESP_OK && scanStatus[SCAN_INTERACTIVE] == ACTION_ENABLE) {
            ESP_LOGE(BLE_TAG, "Error registering BLE GATTC callback: %s", esp_err_to_name(step));
        }
        result |= step;

        step = esp_ble_gattc_app_register(0);
        if (step != ESP_OK && scanStatus[SCAN_INTERACTIVE] == ACTION_ENABLE) {
            ESP_LOGE(BLE_TAG, "Error registering BLE application: %s", esp_err_to_name(step));
        }
        result |= step;

        step = esp_ble_gatt_set_local_mtu(500);
        if (step != ESP_OK && scanStatus[SCAN_INTERACTIVE] == ACTION_ENABLE) {
            ESP_LOGE(BLE_TAG, "Error setting BLE MTU: %s", esp_err_to_name(step));
        }
        result |= step;
        BLE_INITIALISED = true;
    }
    return result;
}

/* Bluetooth Low Energy scanning callback - Called when a BLE device is seen */
static void ble_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    uint8_t *adv_name = NULL;
    uint8_t adv_name_len = 0;
    char bdaStr[MAC_STRLEN + 1];
    wendigo_device dev;
    /* I'm pretty sure these will be initialised because it's on the stack,
       but the consequences of being wrong are too annoying */
    dev.radio.bluetooth.bdname_len = 0;
    dev.radio.bluetooth.eir_len = 0;
    dev.radio.bluetooth.cod = 0;
    dev.radio.bluetooth.bdname = NULL;
    dev.radio.bluetooth.eir = NULL;
    dev.scanType = SCAN_BLE;
    dev.tagged = false;
    switch (event) {
        case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
            esp_ble_gap_start_scanning(CONFIG_BLE_SCAN_SECONDS);
            break;
        case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
            /* This event indicates the end of "scan start" - Did the scan start successfully? */
            if (param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(BLE_TAG, "BLE scan start failed: %s", esp_err_to_name(param->scan_start_cmpl.status));
            } else {
                ESP_LOGI(BLE_TAG, "BLE scan started successfully");
            }
            break;
        case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
            if (param->scan_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(BLE_TAG, "BLE scan stop failed: %s", esp_err_to_name(param->scan_stop_cmpl.status));
            } else {
                ESP_LOGI(BLE_TAG, "BLE scan stopped successfully");
            }
            break;
        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
            if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(BLE_TAG, "BLE advertising stop failed: %s", esp_err_to_name(param->adv_stop_cmpl.status));
            } else {
                ESP_LOGI(BLE_TAG, "BLE advertising stopped successfully.");
            }
            break;
        case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
            mac_bytes_to_string(param->update_conn_params.bda, bdaStr);
            ESP_LOGI(BLE_TAG, "BLE updated connection params for %s. Status: %d, min_int: %d, max_int: %d, conn_int: %d, latency: %d, timeout: %d",
                    bdaStr, param->update_conn_params.status,
                    param->update_conn_params.min_int, param->update_conn_params.max_int,
                    param->update_conn_params.conn_int, param->update_conn_params.latency,
                    param->update_conn_params.timeout);
            break;
        case ESP_GAP_BLE_GET_DEV_NAME_COMPLETE_EVT:
                mac_bytes_to_string(param->scan_rst.bda, bdaStr); //TODO: Confirm param->scan_rst.bda exists
                ESP_LOGI(BLE_TAG, "Got a complete BLE device name for %s: %s. Status %d",
                         bdaStr, param->get_dev_name_cmpl.name, param->get_dev_name_cmpl.status);
                memcpy(dev.mac, param->scan_rst.bda, sizeof(esp_bd_addr_t));
                dev.radio.bluetooth.bdname = (char *)malloc(sizeof(char) * (strlen(param->get_dev_name_cmpl.name) + 1));
                if (dev.radio.bluetooth.bdname == NULL) {
                    outOfMemory();
                    break;
                }
                strncpy(dev.radio.bluetooth.bdname, param->get_dev_name_cmpl.name, strlen(param->get_dev_name_cmpl.name));
                dev.radio.bluetooth.bdname[strlen(param->get_dev_name_cmpl.name)] = '\0';
                // TODO: Can I get anything else out of these structs?
                display_gap_device(&dev);
                /* Add to or update all_gap_devices[] */
                add_device(&dev);
                free_device(&dev);
                break;
        case ESP_GAP_BLE_SCAN_RESULT_EVT:
            esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
            switch (scan_result->scan_rst.search_evt) {
                case ESP_GAP_SEARCH_DISC_RES_EVT:
                    ESP_LOGI(BLE_TAG, "Discovery result");
                    break;
                case ESP_GAP_SEARCH_DISC_BLE_RES_EVT:
                    ESP_LOGI(BLE_TAG, "Discovery BLE result");
                    break;
                case ESP_GAP_SEARCH_INQ_CMPL_EVT:
                    /* Restart the BLE scanner if it hasn't been disabled */
                    if (scanStatus[SCAN_BLE] == ACTION_ENABLE) {
                        esp_ble_gap_start_scanning(CONFIG_BLE_SCAN_SECONDS);
                    }
                    break;
                case ESP_GAP_SEARCH_INQ_RES_EVT:
                    /* Get device info */
                    memcpy(dev.mac, scan_result->scan_rst.bda, sizeof(esp_bd_addr_t));
                    dev.rssi = (int16_t)scan_result->scan_rst.rssi;
                    dev.scanType = SCAN_BLE;
                    /* Name */
                    adv_name = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv,
                                                        ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len);
                    dev.radio.bluetooth.bdname = malloc(sizeof(char) * (adv_name_len + 1));
                    if (dev.radio.bluetooth.bdname == NULL) {
                        outOfMemory();
                        return; // YAGNI: Do something more sophisticated
                    }
                    memcpy(dev.radio.bluetooth.bdname, adv_name, adv_name_len);
                    dev.radio.bluetooth.bdname[adv_name_len] = '\0';
                    dev.radio.bluetooth.bdname_len = adv_name_len;
                    /* Get EIR if provided */
                    if (scan_result->scan_rst.adv_data_len > 0) {
                        dev.radio.bluetooth.eir = malloc(sizeof(uint8_t) * scan_result->scan_rst.adv_data_len);
                        if (dev.radio.bluetooth.eir == NULL) {
                            outOfMemory();
                            return;
                        }
                        memcpy(dev.radio.bluetooth.eir, scan_result->scan_rst.ble_adv, scan_result->scan_rst.adv_data_len);
                        dev.radio.bluetooth.eir_len = scan_result->scan_rst.adv_data_len;
                    }
                    // TODO: Can I find the COD (Class Of Device) anywhere?
                    mac_bytes_to_string(dev.mac, bdaStr);
                    display_gap_device(&dev);
                    /* Add to or update all_gap_devices[] */
                    add_device(&dev);
                    free_device(&dev);
                    break;
                default:
                    break;
            }
        default:
            break;
    }
}

/* BLE GATTC callback. I need to put some time into better understanding what I can do with this */
static void ble_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param) {
    /* If event is register event, store the gattc_if for each profile */
    if (event == ESP_GATTC_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            gl_profile_tab[param->reg.app_id].gattc_if = gattc_if;
        } else {
            ESP_LOGI(BLE_TAG, "reg app failed, app_id %04x, status %d", param->reg.app_id, param->reg.status);
            return;
        }
    }

    /* If the gattc_if equal to profile A, call profile A cb handler,
       so here call each profile's callback */
    do {
        int idx;
        for (idx = 0; idx < PROFILE_NUM; ++idx) {
            if (gattc_if == ESP_GATT_IF_NONE || /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
                    gattc_if == gl_profile_tab[idx].gattc_if) {
                if (gl_profile_tab[idx].gattc_cb) {
                    gl_profile_tab[idx].gattc_cb(event, gattc_if, param);
                }
            }
        }
    } while (0);
}

/* GATTC profile event handler. Called by the above via gl_profile_tab's elements */
static void gattc_profile_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param) {
    esp_ble_gattc_cb_param_t *p_data = (esp_ble_gattc_cb_param_t *)param;

    switch (event) {
        case ESP_GATTC_REG_EVT:
            ESP_LOGI(BLE_TAG, "Event handler registered.");
            esp_err_t scan_ret = esp_ble_gap_set_scan_params(&ble_scan_params);
            if (scan_ret) {
                ESP_LOGE(BLE_TAG, "Set scan params error: %s", esp_err_to_name(scan_ret));
            }
            break;
        case ESP_GATTC_CONNECT_EVT:
            ESP_LOGI(BLE_TAG, "ESP_GATTC_CONNECT_EVT conn_id %d, if %d", p_data->connect.conn_id, gattc_if);
            gl_profile_tab[0].conn_id = p_data->connect.conn_id;
            memcpy(gl_profile_tab[0].remote_bda, p_data->connect.remote_bda, sizeof(esp_bd_addr_t));
            ESP_LOGI(BLE_TAG, "REMOTE BDA:");
            ESP_LOG_BUFFER_HEX(BLE_TAG, gl_profile_tab[0].remote_bda, sizeof(esp_bd_addr_t));
            esp_err_t mtu_ret = esp_ble_gattc_send_mtu_req(gattc_if, p_data->connect.conn_id);
            if (mtu_ret) {
                ESP_LOGE(BLE_TAG, "Config MTU error: %s", esp_err_to_name(mtu_ret));
            }
            break;
        case ESP_GATTC_OPEN_EVT:
            if (param->open.status != ESP_GATT_OK) {
                ESP_LOGE(BLE_TAG, "Open failed, status %s", esp_err_to_name(p_data->open.status));
                break;
            }
            ESP_LOGI(BLE_TAG, "Open success");
            break;
        case ESP_GATTC_DIS_SRVC_CMPL_EVT:
            if (param->dis_srvc_cmpl.status != ESP_GATT_OK) {
                ESP_LOGE(BLE_TAG, "Discover service failed, status: %s", esp_err_to_name(param->dis_srvc_cmpl.status));
                break;
            }
            ESP_LOGI(BLE_TAG, "Discover service complete conn_id %d", param->dis_srvc_cmpl.conn_id);
            esp_ble_gattc_search_service(gattc_if, param->cfg_mtu.conn_id, &remote_filter_service_uuid);
            break;
        case ESP_GATTC_CFG_MTU_EVT:
            if (param->cfg_mtu.status != ESP_GATT_OK) {
                ESP_LOGE(BLE_TAG, "Config MTU failed, error: %s", esp_err_to_name(param->cfg_mtu.status));
            }
            ESP_LOGI(BLE_TAG, "ESP_GATTC_CFG_MTU_EVT, status %d, MTU %d, conn_id %d",
                     param->cfg_mtu.status, param->cfg_mtu.mtu, param->cfg_mtu.conn_id);
            break;
        case ESP_GATTC_SEARCH_RES_EVT:
            ESP_LOGI(BLE_TAG, "SEARCH RES: conn_id = %x is primary service %d",
                    p_data->search_res.conn_id, p_data->search_res.is_primary);
            ESP_LOGI(BLE_TAG, "Start handle %d end handle %d current handle value %d",
                    p_data->search_res.start_handle, p_data->search_res.end_handle, p_data->search_res.srvc_id.inst_id);
            if (p_data->search_res.srvc_id.uuid.len == ESP_UUID_LEN_16 && p_data->search_res.srvc_id.uuid.uuid.uuid16 == REMOTE_SERVICE_UUID) {
                ESP_LOGI(BLE_TAG, "Service found");
                get_server = true;
                gl_profile_tab[0].service_start_handle = p_data->search_res.start_handle;
                gl_profile_tab[0].service_end_handle = p_data->search_res.end_handle;
                ESP_LOGI(BLE_TAG, "UUID16: %x", p_data->search_res.srvc_id.uuid.uuid.uuid16);
            }
            break;
        case ESP_GATTC_SEARCH_CMPL_EVT:
            if (p_data->search_cmpl.status != ESP_GATT_OK) {
                ESP_LOGE(BLE_TAG, "Search service failed, error: %s", esp_err_to_name(p_data->search_cmpl.status));
                break;
            }
            if (p_data->search_cmpl.searched_service_source == ESP_GATT_SERVICE_FROM_REMOTE_DEVICE) {
                ESP_LOGI(BLE_TAG, "Get service information from remote device");
            } else if (p_data->search_cmpl.searched_service_source == ESP_GATT_SERVICE_FROM_NVS_FLASH) {
                ESP_LOGI(BLE_TAG, "Get service information from flash");
            } else {
                ESP_LOGI(BLE_TAG, "Unknown service source");
            }
            ESP_LOGI(BLE_TAG, "ESP_GATTC_SEARCH_CMPL_EVT");
            if (get_server) {
                uint16_t count = 0;
                esp_gatt_status_t status = esp_ble_gattc_get_attr_count(gattc_if,
                                                                        p_data->search_cmpl.conn_id,
                                                                        ESP_GATT_DB_CHARACTERISTIC,
                                                                        gl_profile_tab[0].service_start_handle,
                                                                        gl_profile_tab[0].service_end_handle,
                                                                        0,
                                                                        &count);
                if (status != ESP_GATT_OK) {
                    ESP_LOGE(BLE_TAG, "esp_ble_gattc_get_attr_count error: %s", esp_err_to_name(status));
                }

                if (count > 0) {
                    char_elem_result = (esp_gattc_char_elem_t *)malloc(sizeof(esp_gattc_char_elem_t) * count);
                    if (!char_elem_result) {
                        outOfMemory();
                        ESP_LOGE(BLE_TAG, "gattc no mem");
                    } else {
                        status = esp_ble_gattc_get_char_by_uuid(gattc_if,
                                                                p_data->search_cmpl.conn_id,
                                                                gl_profile_tab[0].service_start_handle,
                                                                gl_profile_tab[0].service_end_handle,
                                                                remote_filter_char_uuid,
                                                                char_elem_result,
                                                                &count);
                        if (status != ESP_GATT_OK) {
                            ESP_LOGE(BLE_TAG, "esp_ble_gattc_get_char_by_uuid error: %s", esp_err_to_name(status));
                        }

                        /* This function was based off an esp-idf bluetooth host example project,
                           I'm unclear on whether there may be more characters available to me - Revisit in future.
                           *****
                           Every service have only one char in our 'ESP_GATTS_DEMO' demo, so we used first 'char_elem_result'
                        */
                        if (count > 0 && (char_elem_result[0].properties & ESP_GATT_CHAR_PROP_BIT_NOTIFY)) {
                            gl_profile_tab[0].char_handle = char_elem_result[0].char_handle;
                            esp_ble_gattc_register_for_notify(gattc_if, gl_profile_tab[0].remote_bda, char_elem_result[0].char_handle);
                        }
                    }
                    free(char_elem_result);
                } else {
                    ESP_LOGE(BLE_TAG, "No char found.");
                }
            }
            break;
        case ESP_GATTC_REG_FOR_NOTIFY_EVT:
            ESP_LOGI(BLE_TAG, "ESP_GATTC_REG_FOR_NOTIFY_EVT");
            if (p_data->reg_for_notify.status != ESP_GATT_OK) {
                ESP_LOGE(BLE_TAG, "REG FOR NOTIFY failed: %s", esp_err_to_name(p_data->reg_for_notify.status));
            } else {
                uint16_t count = 0;
                uint16_t notify_en = 1;
                esp_gatt_status_t ret_status = esp_ble_gattc_get_attr_count(gattc_if,
                                                                            gl_profile_tab[0].conn_id,
                                                                            ESP_GATT_DB_DESCRIPTOR,
                                                                            gl_profile_tab[0].service_start_handle,
                                                                            gl_profile_tab[0].service_end_handle,
                                                                            gl_profile_tab[0].char_handle,
                                                                            &count);
                if (ret_status != ESP_GATT_OK) {
                    ESP_LOGE(BLE_TAG, "esp_ble_gattc_get_attr_count error: %s", esp_err_to_name(ret_status));
                }
                if (count > 0) {
                    descr_elem_result = malloc(sizeof(esp_gattc_descr_elem_t) * count);
                    if (!descr_elem_result) {
                        outOfMemory();
                        ESP_LOGE(BLE_TAG, "gattc no mem");
                    } else {
                        ret_status = esp_ble_gattc_get_descr_by_char_handle(gattc_if,
                                                                            gl_profile_tab[0].conn_id,
                                                                            p_data->reg_for_notify.handle,
                                                                            notify_descr_uuid,
                                                                            descr_elem_result,
                                                                            &count);
                        if (ret_status != ESP_GATT_OK) {
                            ESP_LOGE(BLE_TAG, "esp_ble_gattc_get_descr_by_char_handle error: %s", esp_err_to_name(ret_status));
                        }
                        /* As per previous comment about whether I should try to receive more than a single character */
                        if (count > 0 && descr_elem_result[0].uuid.len == ESP_UUID_LEN_16 && descr_elem_result[0].uuid.uuid.uuid16 == ESP_GATT_UUID_CHAR_CLIENT_CONFIG) {
                            ret_status = esp_ble_gattc_write_char_descr(gattc_if,
                                                                        gl_profile_tab[0].conn_id,
                                                                        descr_elem_result[0].handle,
                                                                        sizeof(notify_en),
                                                                        (uint8_t *)&notify_en,
                                                                        ESP_GATT_WRITE_TYPE_RSP,
                                                                        ESP_GATT_AUTH_REQ_NONE);
                            if (ret_status != ESP_GATT_OK) {
                                ESP_LOGE(BLE_TAG, "esp_ble_gattc_write_char_descr error: %s", esp_err_to_name(ret_status));
                            }
                        }
                        free(descr_elem_result);
                    }
                } else {
                    ESP_LOGE(BLE_TAG, "descr not found");
                }
            }
            break;
        case ESP_GATTC_NOTIFY_EVT:
            if (p_data->notify.is_notify) {
                ESP_LOGI(BLE_TAG, "ESP_GATTC_NOTIFY_EVT, receeive notify value:");
            } else {
                ESP_LOGI(BLE_TAG, "ESP_GATTC_NOTIFY_EVT, receive indicate value:");
            }
            ESP_LOG_BUFFER_HEX(BLE_TAG, p_data->notify.value, p_data->notify.value_len);
            break;
        case ESP_GATTC_WRITE_DESCR_EVT:
            if (p_data->write.status != ESP_GATT_OK) {
                ESP_LOGE(BLE_TAG, "Write descr failed, error: %s", esp_err_to_name(p_data->write.status));
                break;
            }
            ESP_LOGI(BLE_TAG, "Write descr success");
            uint8_t write_char_data[35];
            for (int i = 0; i < sizeof(write_char_data); ++i) {
                write_char_data[i] = i % 256;
            }
            esp_ble_gattc_write_char(gattc_if,
                                     gl_profile_tab[0].conn_id,
                                     gl_profile_tab[0].char_handle,
                                     sizeof(write_char_data),
                                     write_char_data,
                                     ESP_GATT_WRITE_TYPE_RSP,
                                     ESP_GATT_AUTH_REQ_NONE);
            break;
        case ESP_GATTC_SRVC_CHG_EVT:
            esp_bd_addr_t bda;
            memcpy(bda, p_data->srvc_chg.remote_bda, sizeof(esp_bd_addr_t));
            ESP_LOGI(BLE_TAG, "ESP_GATTC_SRVC_CHG_EVT, bd_addr: ");
            ESP_LOG_BUFFER_HEX(BLE_TAG, bda, sizeof(esp_bd_addr_t));
            break;
        case ESP_GATTC_WRITE_CHAR_EVT:
            if (p_data->write.status != ESP_GATT_OK) {
                ESP_LOGE(BLE_TAG, "Write char failed, error: %s", esp_err_to_name(p_data->write.status));
                break;
            }
            ESP_LOGI(BLE_TAG, "Write char success");
            break;
        case ESP_GATTC_DISCONNECT_EVT:
            connect = false;
            get_server = false;
            ESP_LOGI(BLE_TAG, "ESP_GATTC_DISCONNECT_EVT, reason = %d", p_data->disconnect.reason);
            break;
        default:
            break;
    }
}

wendigo_device *device_from_gap_cb(esp_bt_gap_cb_param_t *param) {
    wendigo_device *dev = (wendigo_device *)malloc(sizeof(wendigo_device));
    dev->radio.bluetooth.eir_len = 0;
    dev->radio.bluetooth.bdname_len = 0;
    dev->radio.bluetooth.bdname = NULL;
    dev->radio.bluetooth.eir = NULL;
    dev->radio.bluetooth.cod = 0;
    dev->scanType = SCAN_HCI;
    dev->tagged = false;
    esp_bt_gap_dev_prop_t *p;
    char codDevType[COD_MAX_LEN]; /* Placeholder to store COD major device type */
    uint8_t codDevTypeLen = 0;
    char dev_bdname[ESP_BT_GAP_MAX_BDNAME_LEN + 1];
    
    memcpy(dev->mac, param->disc_res.bda, ESP_BD_ADDR_LEN);

    /* Collect all the properties we have */
    int numProp = param->disc_res.num_prop;
    for (int i = 0; i < numProp; ++i) {
        p = param->disc_res.prop + i;
        switch (p->type) {
            case ESP_BT_GAP_DEV_PROP_COD:
                dev->radio.bluetooth.cod = *(uint32_t *)(p->val);
                cod2deviceStr(dev->radio.bluetooth.cod, codDevType, &codDevTypeLen);
                break;
            case ESP_BT_GAP_DEV_PROP_RSSI:
                dev->rssi = *(int16_t *)(p->val);
                break;
            case ESP_BT_GAP_DEV_PROP_BDNAME:
                dev->radio.bluetooth.bdname_len = (p->len > ESP_BT_GAP_MAX_BDNAME_LEN) ?
                                                   ESP_BT_GAP_MAX_BDNAME_LEN : (uint8_t)p->len;
                dev->radio.bluetooth.bdname = (char *)malloc(sizeof(char) *
                                               (dev->radio.bluetooth.bdname_len + 1));
                if (dev->radio.bluetooth.bdname == NULL) {
                    outOfMemory();
                    break;
                }
                memcpy(dev->radio.bluetooth.bdname, p->val, dev->radio.bluetooth.bdname_len);
                dev->radio.bluetooth.bdname[dev->radio.bluetooth.bdname_len] = '\0';
                break;
            case ESP_BT_GAP_DEV_PROP_EIR:
                dev->radio.bluetooth.eir = (uint8_t *)malloc(sizeof(uint8_t) * p->len);
                if (dev->radio.bluetooth.eir == NULL) {
                    outOfMemory();
                    break;
                }
                memcpy(dev->radio.bluetooth.eir, p->val, p->len);
                dev->radio.bluetooth.eir_len = p->len;
                break;
            default:
                break;
        }
    }

    /* If we didn't get a bdname check EIR for a name */
    if (dev->radio.bluetooth.bdname_len == 0) {
        get_string_name_from_eir(dev->radio.bluetooth.eir, dev_bdname, &(dev->radio.bluetooth.bdname_len));
        if (dev->radio.bluetooth.bdname_len > 0) {
            dev->radio.bluetooth.bdname = (char *)malloc(sizeof(char) * (dev->radio.bluetooth.bdname_len + 1));
            strncpy(dev->radio.bluetooth.bdname, dev_bdname, dev->radio.bluetooth.bdname_len);
            dev->radio.bluetooth.bdname[dev->radio.bluetooth.bdname_len] = '\0';
        }
    }
    return dev;
}

/* Bluetooth Classic Discovery callback - Called on Bluetooth Classic events including
   device discovery, additional device information, discovery completed */
static void bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
    switch (event) {
        case ESP_BT_GAP_DISC_RES_EVT:
            char bdaStr[MAC_STRLEN + 1];
            mac_bytes_to_string(param->disc_res.bda, bdaStr);
            wendigo_device *dev = device_from_gap_cb(param);
            if (dev == NULL) {
                ESP_LOGE(BT_TAG, "Failed to obtain device from event parameters :(");
            }
            display_gap_device(dev);
            /* Add to or update all_gap_devices[] */
            add_device(dev);
            free_device(dev);
            free(dev);
            break;
        case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
            if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED) {
                ESP_LOGI(BT_TAG, "ESP_BT_GAP_DSC_STATE_CHANGED_EVT: ESP_BT_GAP_DISCOVERY_STOPPED:");
                if (scanStatus[SCAN_HCI] == ACTION_ENABLE) {
                    ESP_LOGI(BT_TAG, "Restarting...");
                    esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, CONFIG_BT_SCAN_DURATION, 0);
                }
            } else if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STARTED) {
                ESP_LOGI(BT_TAG, "ESP_BT_GAP_DISC_STATE_CHANGED_EVT: ESP_BT_GAP_DISCOVERY_STARTED");
            }
            break;
        case ESP_BT_GAP_RMT_SRVCS_EVT:
            /* Found a service on a remote device. Parse and decode the service */
            // TODO: bt_remote_service_cb(param);
            break;
        case ESP_BT_GAP_RMT_SRVC_REC_EVT:
        default:
            ESP_LOGI(BT_TAG, "event: %d", event);
            break;
    }
    return;
}

esp_err_t wendigo_bt_gap_start() {
    esp_err_t err = ESP_OK;

    err |= esp_bt_gap_register_callback(bt_gap_cb);
    /* Set Device name */
    char *dev_name = "WENDIGO_INQUIRY";
    err |= esp_bt_gap_set_device_name(dev_name);

    /* Set discoverable and connectable, wait to be connected */
    err |= esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);

    /* Start to discover nearby devices */
    err |= esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, CONFIG_BT_SCAN_DURATION, 0);

    return err;
}

esp_err_t wendigo_bt_enable() {
    esp_err_t result = ESP_OK;
    if (!BT_INITIALISED) {
        result = wendigo_bt_initialise();
        if (result != ESP_OK) {
            return result;
        }
    }
    return wendigo_bt_gap_start();
}

/* This function does not terminate in-flight discovery. Its calling function -
   cmd_bluetooth in wendigo.c - sets scanStatus[SCAN_HCI] to ACTION_DISABLE.
   When the current discovery interval expires a new discovery task will not be launched */
esp_err_t wendigo_bt_disable() {
    esp_err_t result = ESP_OK;
    if (!BT_INITIALISED) {
        result = wendigo_bt_initialise();
        if (result != ESP_OK) {
            return result;
        }
    }
    return result;
}

esp_err_t wendigo_ble_enable() {
    esp_err_t result = ESP_OK;
    if (!BLE_INITIALISED) {
        result = wendigo_ble_initialise();
        if (result != ESP_OK) {
            return result;
        }
    }
    return esp_ble_gap_start_scanning(CONFIG_BLE_SCAN_SECONDS);
}

esp_err_t wendigo_ble_disable() {
    esp_err_t result = ESP_OK;
    if (!BLE_INITIALISED) {
        result = wendigo_ble_initialise();
        if (result != ESP_OK) {
            return result;
        }
    }
    return esp_ble_gap_stop_scanning();
}

/* cod2deviceStr
   Converts a uint32_t representing a Bluetooth Class Of Device (COD)'s major
   device code into a useful string descriptor of the specified COD major device.
   Returns an error indicator. Input 'cod' is the COD to be converted.
   char *string is a pointer to a character array of sufficient size to hold
   the results. The maximum possible number of characters required by this
   function, including the trailing NULL, is 59.
   uint8_t *stringLen is a pointer to a uint8_t variable in the calling code.
   This variable will be overwritten with the length of string.
*/
esp_err_t cod2deviceStr(uint32_t cod, char *string, uint8_t *stringLen) {
    esp_err_t err = ESP_OK;
    char temp[COD_MAX_LEN] = "";
    esp_bt_cod_major_dev_t devType = esp_bt_gap_get_cod_major_dev(cod);
    switch (devType) {
        case ESP_BT_COD_MAJOR_DEV_MISC:
            strcpy(temp, "Miscellaneous");
            break;
        case ESP_BT_COD_MAJOR_DEV_COMPUTER:
            strcpy(temp, "Computer");
            break;
        case ESP_BT_COD_MAJOR_DEV_PHONE:
            strcpy(temp, "Phone (cellular, cordless, pay phone, modem)");
            break;
        case ESP_BT_COD_MAJOR_DEV_LAN_NAP:
            strcpy(temp, "LAN, Network Access Point");
            break;
        case ESP_BT_COD_MAJOR_DEV_AV:
            strcpy(temp, "Audio/Video (headset, speaker, stereo, video display, VCR)");
            break;
        case ESP_BT_COD_MAJOR_DEV_PERIPHERAL:
            strcpy(temp, "Peripheral (mouse, joystick, keyboard)");
            break;
        case ESP_BT_COD_MAJOR_DEV_IMAGING:
            strcpy(temp, "Imaging (printer, scanner, camera, display)");
            break;
        case ESP_BT_COD_MAJOR_DEV_WEARABLE:
            strcpy(temp, "Wearable");
            break;
        case ESP_BT_COD_MAJOR_DEV_TOY:
            strcpy(temp, "Toy");
            break;
        case ESP_BT_COD_MAJOR_DEV_HEALTH:
            strcpy(temp, "Health");
            break;
        case ESP_BT_COD_MAJOR_DEV_UNCATEGORIZED:
            strcpy(temp, "Uncategorised: Device not specified");
            break;
        default:
            strcpy(temp, "ERROR: Invalid Major Device Type");
            break;
    }
    strcpy(string, temp);
    *stringLen = strlen(string);

    return err;
}

/* As above, but returning a shortened string suitable for display by VIEW */
esp_err_t cod2shortStr(uint32_t cod, char *string, uint8_t *stringLen) {
    esp_err_t err = ESP_OK;
    char temp[SHORT_COD_MAX_LEN] = "";
    esp_bt_cod_major_dev_t devType = esp_bt_gap_get_cod_major_dev(cod);
    switch (devType) {
        case ESP_BT_COD_MAJOR_DEV_MISC:
            strcpy(temp, "Misc.");
            break;
        case ESP_BT_COD_MAJOR_DEV_COMPUTER:
            strcpy(temp, "PC");
            break;
        case ESP_BT_COD_MAJOR_DEV_PHONE:
            strcpy(temp, "Phone");
            break;
        case ESP_BT_COD_MAJOR_DEV_LAN_NAP:
            strcpy(temp, "LAN");
            break;
        case ESP_BT_COD_MAJOR_DEV_AV:
            strcpy(temp, "A/V");
            break;
        case ESP_BT_COD_MAJOR_DEV_PERIPHERAL:
            strcpy(temp, "Peripheral");
            break;
        case ESP_BT_COD_MAJOR_DEV_IMAGING:
            strcpy(temp, "Imaging");
            break;
        case ESP_BT_COD_MAJOR_DEV_WEARABLE:
            strcpy(temp, "Wearable");
            break;
        case ESP_BT_COD_MAJOR_DEV_TOY:
            strcpy(temp, "Toy");
            break;
        case ESP_BT_COD_MAJOR_DEV_HEALTH:
            strcpy(temp, "Health");
            break;
        case ESP_BT_COD_MAJOR_DEV_UNCATEGORIZED:
            strcpy(temp, "Uncat.");
            break;
        default:
            strcpy(temp, "Invalid");
            break;
    }
    strcpy(string, temp);
    *stringLen = strlen(string);

    return err;
}

/* Convert a UUID (service or attribute descriptor) to a printable hex string */
char *uuid2str(esp_bt_uuid_t *uuid, char *str, size_t size) {
    if (uuid == NULL || str == NULL) {
        return NULL;
    }

    if (uuid->len == 2 && size >= 5) {
        sprintf(str, "%04x", uuid->uuid.uuid16);
    } else if (uuid->len == 4 && size >= 9) {
        sprintf(str, "%08"PRIx32, uuid->uuid.uuid32);
    } else if (uuid->len == 16 && size >= 37) {
        uint8_t *p = uuid->uuid.uuid128;
        sprintf(str, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                p[15], p[14], p[13], p[12], p[11], p[10], p[9], p[8], p[7], p[6], p[5], p[4],
                p[3], p[2], p[1], p[0]);
    } else {
        return NULL;
    }
    return str;
}

static bool get_name_from_eir(uint8_t *eir, uint8_t *bdname, uint8_t *bdname_len)
{
    uint8_t *rmt_bdname = NULL;
    uint8_t rmt_bdname_len = 0;

    if (!eir) {
        return false;
    }

    rmt_bdname = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &rmt_bdname_len);
    if (!rmt_bdname) {
        rmt_bdname = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &rmt_bdname_len);
    }

    if (rmt_bdname) {
        if (rmt_bdname_len > ESP_BT_GAP_MAX_BDNAME_LEN) {
            rmt_bdname_len = ESP_BT_GAP_MAX_BDNAME_LEN;
        }

        if (bdname) {
            memcpy(bdname, rmt_bdname, rmt_bdname_len);
            bdname[rmt_bdname_len] = '\0';
        }
        if (bdname_len) {
            *bdname_len = rmt_bdname_len;
        }
        return true;
    }
    return false;
}

static bool get_string_name_from_eir(uint8_t *eir, char *bdname, uint8_t *bdname_len) {
    bool retVal = true;
    uint8_t bdname_bytes[ESP_BT_GAP_MAX_BDNAME_LEN + 1]; // TODO: Remove +1?
    retVal = get_name_from_eir(eir, bdname_bytes, bdname_len);

    if (*bdname_len > 0) {
        memcpy(bdname, bdname_bytes, *bdname_len);
        bdname[*bdname_len] = '\0';
    }
    return retVal;
}

/* Search the store of known UUIDs for the specified UUID */
bt_uuid *svcForUUID(uint16_t uuid) {
    uint8_t svcIdx = 0;
    for ( ; svcIdx < BT_UUID_COUNT && uuid != bt_uuids[svcIdx].uuid16; ++ svcIdx) {}
    if (svcIdx == BT_UUID_COUNT) {
        return NULL;
    }
    return &(bt_uuids[svcIdx]);
}
