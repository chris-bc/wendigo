#include "bluetooth.h"
#include "uuids.c"

bool BT_INITIALISED = false;

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

esp_err_t wendigo_bt_enable() {
    esp_err_t result = ESP_OK;
    if (!BT_INITIALISED) {
        result = wendigo_bt_initialise();
        if (result != ESP_OK) {
            return result;
        }
    }
    return result;
}

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
    if (!BT_INITIALISED) {
        result = wendigo_bt_initialise();
        if (result != ESP_OK) {
            return result;
        }
    }
    return result;
}

esp_err_t wendigo_ble_disable() {
    esp_err_t result = ESP_OK;
    if (!BT_INITIALISED) {
        result = wendigo_bt_initialise();
        if (result != ESP_OK) {
            return result;
        }
    }
    return result;
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
    char temp[59] = "";
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
    char temp[11] = "";
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
            #ifdef CONFIG_FLIPPER
                strcpy(temp, "Periph.");
            #else
                strcpy(temp, "Peripheral");
            #endif
            break;
        case ESP_BT_COD_MAJOR_DEV_IMAGING:
            strcpy(temp, "Imaging");
            break;
        case ESP_BT_COD_MAJOR_DEV_WEARABLE:
            strcpy(temp, "Wearable");
            #ifdef CONFIG_FLIPPER
                temp[7] = '\0'; /* Truncate the last character if necessary */
            #endif
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

/* Convert Bluetooth Device Address (equivalent to MAC) to a printable hex string */
char *bda2str(esp_bd_addr_t bda, char *str, size_t size) {
    if (bda == NULL || str == NULL || size < (MAC_STRLEN + 1)) {
        return NULL;
    }

    uint8_t *p = bda;
    sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x", p[0], p[1], p[2], p[3], p[4], p[5]);
    return str;
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
