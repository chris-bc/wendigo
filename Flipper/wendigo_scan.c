#include "wendigo_scan.h"

uint8_t *buffer = NULL;
uint16_t bufferLen = 0; // 65535 should be plenty of length
uint16_t bufferCap = 0; // Buffer capacity - I don't want to allocate 65kb, but don't want to constantly realloc

/* Packet identifiers */
uint8_t PREAMBLE_LEN = 8;
uint8_t PREAMBLE_BT_BLE[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xAA, 0xAA, 0xAA, 0xAA};
uint8_t PREAMBLE_WIFI[]   = {0x99, 0x99, 0x99, 0x99, 0x11, 0x11, 0x11, 0x11};
uint8_t PREAMBLE_VER[]    = {'W', 'e', 'n', 'd', 'i', 'g', 'o', ' '};

/* Enable or disable Wendigo scanning on all interfaces, using app->interfaces
   to determine which radios should be enabled/disabled when starting to scan.
   This function is called by the UI handler (wendigo_scene_start) when scanning
   is started or stopped. app->interfaces is updated via the Setup menu.
   Because wendigo_uart_tx returns void there is no way to identify failure,
   so this function also returns void.
*/
void wendigo_set_scanning_active(WendigoApp *app, bool starting) {
    char cmd;
    uint8_t arg;
    const uint8_t CMD_LEN = 5; // e.g. "b 1\n\0"
    char cmdString[CMD_LEN];
    for (int i = 0; i < IF_COUNT; ++i) {
        /* Set command */
        cmd = (i == IF_BLE) ? 'b' : (i == IF_BT_CLASSIC) ? 'h' : 'w';
        /* arg */
        arg = (starting && app->interfaces[i].active) ? 1 : 0;
        snprintf(cmdString, CMD_LEN, "%c %d\n", cmd, arg);
        wendigo_uart_tx(app->uart, (uint8_t *)cmdString, CMD_LEN);
    }
    app->is_scanning = starting;
}

/* Returns the number of bytes consumed from the buffer - DOES NOT
   remove consumed bytes from the buffer, this must be handled by
   the calling function. */
uint16_t parseBufferBluetooth(WendigoApp *app) {
    // TODO
    UNUSED(app);
    return 0;
}

/* Returns the number of bytes consumed from the buffer - DOES NOT
   remove consumed bytes, this must be handled by the calling function. */
uint16_t parseBufferWifi(WendigoApp *app) {
    // TODO
    UNUSED(app);
    return 0;
}

/* Returns the number of bytes consumed from the buffer - DOES NOT
   remove consumed bytes, this must be handled by the calling function. */
uint16_t parseBufferVersion(WendigoApp *app) {
    // TODO
    // Create a char[] of bufferLen
    // Copy bytes across, replacing newline with NULL terminator
    // Call a UI method (can I access wendigo_scene_help's scope from here?) to display a popup
    char versionStr[bufferLen];
    int i = 0;
    for (; i < bufferLen && buffer[i] != '\n'; ++i) {
        versionStr[i] = buffer[i];
    }
    // TODO: Consider handling a missing newline more elegantly - Although it's currently
    //       the end-of-transmission marker so not possible to get here without one
    if (i == bufferLen) {
        --i; // Replace the last byte with NULL. This should never happen.
    }
    versionStr[i] = '\0';
    wendigo_display_popup(app, "ESP32 Version", versionStr);
    return i + 1;
}

/* When the end of a transmission is reached this function is called to parse the
   buffer and take the appropriate action. We expect to see one of the following:
    * Begin with 0xFF,0xFF,0xFF,0xFF,0xAA,0xAA,0xAA,0xAA: Bluetooth packet
    * Begin with 0x99,0x99,0x99,0x99,0x11,0x11,0x11,0x11: WiFi packet
    * Begin with "Wendigo "                              : Version message
*/
void parseBuffer(WendigoApp *app) {
    /* It should be impossible to reach here with fewer than 8 bytes in the buffer */
    if (bufferLen < 8) {
        // TODO: Panic
        return;
    }
    uint16_t consumedBytes = 0;
    if (!memcmp(PREAMBLE_BT_BLE, buffer, PREAMBLE_LEN)) {
        consumedBytes = parseBufferBluetooth(app);
    } else if (!memcmp(PREAMBLE_WIFI, buffer, PREAMBLE_LEN)) {
        consumedBytes = parseBufferWifi(app);
    } else if (!memcmp(PREAMBLE_VER, buffer, PREAMBLE_LEN)) {
        consumedBytes = parseBufferVersion(app);
    } else {
        // TODO: Panic
    }
    if (consumedBytes == 0) {
        // That was a bit of a waste
        // TODO: Diagnose why, communicate something useful
        return;
    }
    /* Remove `consumedBytes` bytes from the beginning of the buffer */
    memset(buffer, '\0', consumedBytes);
    /* Copy memory into buffer from buffer+consumedBytes to buffer+bufferLen.
       This should be OK when the buffer is exhausted because it copies 0 bytes */
    memcpy(buffer, buffer + consumedBytes, bufferLen - consumedBytes);
    /* Null what remains */
    memset(buffer + bufferLen - consumedBytes, '\0', consumedBytes);
    /* Buffer is now bufferLen - consumedBytes */
    bufferLen -= consumedBytes;
}

/* Callback invoked when UART data is received. When an end-of-message packet is
   received it is sent to the appropriate handler for display.
    (NOTE: At the time of writing the very fancy-sounding "end-of-message packet"
           is a newline ('\n'))
   At the time of writing a similar callback exists in wendigo_scene_console_output,
   that is being retained through initial stages of development because it provides
   a useful way to run ad-hoc tests from Flipper, with "List Devices" currently
   providing the ability to send arbitrary UART commands and display results to the
   console. Eventually the entire scene wendigo_scene_console_output will likely be
   removed, until then collision can be avoided by limiting its use, ensuring that
   wendigo_uart_set_handle_rx_data_cb() is called after using the console to
   re-establish this function as the UART callback.
*/
void wendigo_scan_handle_rx_data_cb(uint8_t* buf, size_t len, void* context) {
    furi_assert(context);
    WendigoApp* app = context;

    /* Extend the buffer if necessary */
    if (bufferLen + len >= bufferCap) {
        /* Extend it by the larger of len and 128 - wendigo_bt_device is 72 bytes + name + EIR + preamble */
        uint16_t newCapacity = bufferCap + ((len > 128) ? len : 128);
        uint8_t *newBuffer = realloc(buffer, newCapacity); // Behaves like malloc() when buffer==NULL
        if (newBuffer == NULL) {
            /* Out of memory */
            // TODO: Panic
            return;
        }
        buffer = newBuffer;
        bufferCap = newCapacity;
    }
    memcpy(buffer + bufferLen, buf, len);
    bufferLen += len;

    /* Have we reached the end of transmission? */
    if (buffer[bufferLen - 1] == '\n') { // TODO: Test whether multiple messages can be received at once even though stdout is flushed after each message
                                         //       Or a complete message followed by part of the next message (which would require finding a newline in
                                         //       the buffer, parsing the first part and shuffling bytes forward).
        parseBuffer(app);
    }

}

void wendigo_free_uart_buffer() {
    if (bufferCap > 0) {
        free(buffer);
        bufferLen = 0;
        bufferCap = 0;
        buffer = NULL;
    }
}

/* Send the version command to ESP32 */
void wendigo_esp_version(WendigoApp *app) {
    char cmd[] = "v\n";
    wendigo_uart_tx(app->uart, (uint8_t *)cmd, strlen(cmd) + 1);
}