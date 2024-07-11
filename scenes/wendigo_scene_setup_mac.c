#include "../wendigo_app_i.h"

/* Local buffer for use of the view */
uint8_t view_bytes[NUM_MAC_BYTES];

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
    // If MAC has changed
    //   If immutable
    //     Popup saying it can't be changed
    //   else
    //     Popup indicating success or failure
    WendigoApp *app = context;

    /* Did the user change the MAC? */
    //if (!memcmp(view_bytes, app->mac_bytes, NUM_MAC_BYTES)) 

    /* Configure popup */
    popup_set_header(
        app->popup,
        "Update MAC",
        1,
        1,
        AlignCenter,
        AlignTop
    );
    popup_set_text(
        app->popup,
        "Your message here",
        4,
        4,
        AlignCenter,
        AlignBottom
    );
    popup_set_icon(app->popup, -1, -1, NULL);
    popup_set_timeout(app->popup, 2000); // 2 secondsn
    popup_enable_timeout(app->popup);
    //popup_set_callback(app->popup, NULL);
    popup_set_context(app->popup, app);
    
    view_dispatcher_switch_to_view(app->view_dispatcher, WendigoAppViewPopup);
}

void wendigo_scene_setup_mac_on_enter(void *context) {
    WendigoApp *app = context;
    ByteInput *mac_input = app->setup_mac;

    /* Copy app->mac_bytes into a temp array for modification by the view */
    memcpy(view_bytes, app->mac_bytes, NUM_MAC_BYTES);
    
    byte_input_set_header_text(mac_input, "MAC Address");
    byte_input_set_result_callback(
        mac_input,
        wendigo_scene_setup_mac_input_callback,
        NULL,
        app,
        view_bytes,
        NUM_MAC_BYTES
    );
    view_dispatcher_switch_to_view(app->view_dispatcher, WendigoAppViewSetupMAC);
}

bool wendigo_scene_setup_mac_on_event(void *context, SceneManagerEvent event) {
    //
    UNUSED(context);
    UNUSED(event);
    return true;
}

void wendigo_scene_setup_mac_on_exit(void *context) {
    UNUSED(context);
}