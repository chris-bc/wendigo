#include "common.h"

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
