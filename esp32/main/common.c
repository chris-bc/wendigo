#include "common.h"

/* Display a simple out of memory message and set error code */
esp_err_t outOfMemory() {
    printf("Out of memory\n");
    return ESP_ERR_NO_MEM;
}

/* Return the specified response over UART */
esp_err_t send_response(char *cmd, char *arg, MsgType result) {
    char resultMsg[8];
    switch (result) {
        case MSG_ACK:
            strcpy(resultMsg, "ACK");
            break;
        case MSG_OK:
            strcpy(resultMsg, "OK");
            break;
        case MSG_FAIL:
            strcpy(resultMsg, "FAIL");
            break;
        case MSG_INVALID:
            strcpy(resultMsg, "INVALID");
            break;
        default:
            ESP_LOGE(TAG, "Invalid result type: %d\n", result);
            return ESP_ERR_INVALID_ARG;
    }
    if (arg == NULL) {
        printf("%s %s\n", cmd, resultMsg);
    } else {
        printf("%s %s %s\n", cmd, arg, resultMsg);
    }
    return ESP_OK;
}

/* Command syntax is <command> <ActionType>, where ActionType :== 0 | 1 | 2 */
ActionType parseCommand(int argc, char **argv) {
    /* Validate the argument - Must be between 0 & 2 */
    if (argc != 2) {
        return ACTION_INVALID;
    }
    int action = strtol(argv[1], NULL, 10);
    if (strlen(argv[1]) == 1 && action >= ACTION_DISABLE && action < ACTION_INVALID) {
        return (ActionType)action;
    } else {
        return ACTION_INVALID;
    }
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

/* Convert the specified byte array to a string representing
   a MAC address. strMac must be a pointer initialised to
   contain at least 18 bytes (MAC + '\0') */
   esp_err_t mac_bytes_to_string(uint8_t *bMac, char *strMac) {
    sprintf(strMac, "%02X:%02X:%02X:%02X:%02X:%02X", bMac[0],
            bMac[1], bMac[2], bMac[3], bMac[4], bMac[5]);
    return ESP_OK;
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
