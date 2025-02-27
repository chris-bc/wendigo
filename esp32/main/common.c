#include "common.h"
#include "esp_err.h"

const char *TAG = "WENDIGO";
const uint8_t BROADCAST[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

/* Common string definitions */
char STRINGS_HOP_STATE_FAIL[] = "Unable to set hop state: ";
char STRINGS_MALLOC_FAIL[] = "Unable to allocate memory ";
char STRINGS_SET_MAC_FAIL[] = "Unable to set MAC ";
char STRINGS_HOPMODE_INVALID[] = "Invalid hopMode ";

bool arrayContainsString(char **arr, int arrCnt, char *str) {
    int i;
    for (i=0; i < arrCnt && strcmp(arr[i], str); ++i) { }
    return (i != arrCnt);
}

/* Convert the specified string to a byte array
   bMac must be a pointer to 6 bytes of allocated memory */
esp_err_t mac_string_to_bytes(char *strMac, uint8_t *bMac) {
    uint8_t nBytes = (strlen(strMac) + 1) / 3; /* Support arbitrary-length string */
    char *working = strMac;
    if (nBytes == 0) {
        #ifdef CONFIG_FLIPPER
            printf("mac_string_to_bytes()\ninvalid input\n   \"%s\"\n", strMac);
        #else
            ESP_LOGE(TAG, "mac_string_to_bytes() called with an invalid string - There are no bytes\n\t%s\tExpected format 0A:1B:2C:3D:4E:5F:60", strMac);
        #endif
        return ESP_ERR_INVALID_ARG;
    }
    for (int i = 0; i < nBytes; ++i, ++working) {
        bMac[i] = strtol(working, &working, 10);
    }
    return ESP_OK;
}

/* Convert the specified byte array to a string representing
   a MAC address. strMac must be a pointer initialised to
   contain at least 18 bytes (MAC + '\0') */
esp_err_t mac_bytes_to_string(uint8_t *bMac, char *strMac) {
    sprintf(strMac, "%02X:%02X:%02X:%02X:%02X:%02X", bMac[0],
            bMac[1], bMac[2], bMac[3], bMac[4], bMac[5]);
    return ESP_OK;
}

/* General purpose byte to string convertor
   byteCount specifies the number of bytes to be converted to a string,
   commencing at bytes.
   string must be an initialised char[] containing sufficient space
   for the results.
   String length (including terminating \0) will be 3 * byteCount
   (standard formatting - 0F:AA:E5)
*/
esp_err_t bytes_to_string(uint8_t *bytes, char *string, int byteCount) {
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

/* Retain current MAC */
static uint8_t current_mac[6] = {0, 0, 0, 0, 0, 0};

uint8_t *wendigo_get_mac() {
    if (current_mac[0] == 0 && current_mac[1] == 0 && current_mac[2] == 0 && current_mac[3] == 0 && current_mac[4] == 0 && current_mac[5] == 0) {
        if (esp_wifi_get_mac(WIFI_IF_AP, current_mac) != ESP_OK) {
            #ifdef CONFIG_FLIPPER
                printf("Failed to get MAC\n");
            #else
                ESP_LOGE(TAG, "Failed to get MAC!");
            #endif
            return NULL;
        }
    }
    return current_mac;
}

esp_err_t wendigo_set_mac(uint8_t *newMac) {
    if (esp_wifi_set_mac(ESP_IF_WIFI_AP, newMac) != ESP_OK) {
        #ifdef CONFIG_FLIPPER
            printf("Failed to set MAC\n");
        #else
            ESP_LOGE(TAG, "Failed to set MAC");
        #endif
        return ESP_ERR_WIFI_MAC;
    }
    #ifdef CONFIG_DEBUG
        #ifdef CONFIG_FLIPPER
            printf("Wendigo MAC now: %02x:%02x:%02x:%02x:%02x:%02x\n", newMac[0], newMac[1], newMac[2], newMac[3], newMac[4], newMac[5]);
        #else
            ESP_LOGI(TAG, "Successfully updated Wendigo MAC to %02x:%02x:%02x:%02x:%02x:%02x\n", newMac[0], newMac[1], newMac[2], newMac[3], newMac[4], newMac[5]);
        #endif
    #endif
    memcpy(current_mac, newMac, 6);
    return ESP_OK;
}

esp_err_t convert_bytes_to_string(uint8_t *bIn, char *sOut, int maxLen) {
    memset(sOut, '\0', maxLen + 1);
    for (int i = 0; i < maxLen + 1 && bIn[i] != '\0'; ++i) {
        sOut[i] = bIn[i];
    }
    return ESP_OK;
}

/* Convert */
esp_err_t ssid_bytes_to_string(uint8_t *bSsid, char *ssid) {
    return convert_bytes_to_string(bSsid, ssid, MAX_SSID_LEN);
}
