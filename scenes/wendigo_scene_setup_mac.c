#include "../wendigo_app_i.h"

/* Convert an array of byteCount uint8_ts into a colon-separated string of bytes.
   strBytes must be initialised with sufficient space to hold the output string.
   For a MAC this is 18 bytes. In general it is 3 * byteCount */
void bytes_to_string(uint8_t *bytes, uint16_t bytesCount, char *strBytes) {
    uint8_t *p_in = bytes;
    const char *hex = "0123456789ABCDEF";
    char *p_out = strBytes;
    for (; p_in < bytes + bytesCount; p_out += 3, ++p_in) {
        p_out[0] = hex[(*p_in >> 4) & 0xF];
        p_out[1] = hex[*p_in & 0xF];
        p_out[2] = ':';
    }
    p_out[-1] = 0;
}

void wendigo_scene_setup_mac_input_callback(void *context) {
    //
    UNUSED(context);
}

void wendigo_scene_setup_mac_changed_callback(void *context) {
    //
    UNUSED(context);
}

void wendigo_scene_setup_mac_on_enter(void *context) {
    //
    UNUSED(context);
}

bool wendigo_scene_setup_mac_on_event(void *context, SceneManagerEvent event) {
    //
    UNUSED(context);
    UNUSED(event);
    return true;
}

void wendigo_scene_setup_mac_on_exit(void *context) {
    //
    UNUSED(context);
}