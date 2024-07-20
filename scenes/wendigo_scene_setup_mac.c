#include "../wendigo_app_i.h"

#define BLUETOOTH_MAC_IS_MUTABLE (false)
#define WIFI_MAC_IS_MUTABLE (true)
#define IF_MAX_LEN (10) /* Maximum length of an interface string ("WiFi", "Bluetooth", etc.) */

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
    if (memcmp(view_bytes, app->mac_bytes, NUM_MAC_BYTES)) {
        char result_if_text[IF_MAX_LEN] = "";
        char popup_header_text[IF_MAX_LEN + 11] = "";
        char popup_text[IF_MAX_LEN + 50] = "";
        /* MAC was changed - was that allowed? */
        /* Set interface string for popups */
        switch (app->mac_interface) {
            case IF_BLUETOOTH:
                strcpy(result_if_text, "Bluetooth");
                break;
            case IF_WIFI:
                strcpy(result_if_text, "WiFi");
                break;
        }
        snprintf(popup_header_text, strlen("Update  MAC") + strlen(result_if_text) + 1,
                    "Update %s MAC", result_if_text);
        /* Is the MAC mutable? */
        if ((app->mac_interface == IF_BLUETOOTH && BLUETOOTH_MAC_IS_MUTABLE) ||
                        (app->mac_interface == IF_WIFI && WIFI_MAC_IS_MUTABLE)) {
            /* MAC is mutable. Update the relevant MAC */
            memcpy(app->mac_bytes, view_bytes, NUM_MAC_BYTES);
            bool success = true;
            switch (app->mac_interface) {
                case IF_BLUETOOTH:
                    // TODO: Update BT MAC
                    break;
                case IF_WIFI:
                    // TODO: Update WiFi MAC
                    // success = blah blah blah
                    break;
            }
            /* Build popup text */
            if (success) {
                /* Success popup */
                snprintf(popup_text, strlen(result_if_text) + strlen(" MAC Updated!") + 1,
                    "%s MAC Updated!", result_if_text);
            } else {
                /* Failure popup */
                snprintf(popup_text, strlen(result_if_text) + strlen("Failed to Update  MAC!") + 1,
                    "Failed to Update %s MAC!", result_if_text);
            }
        } else {
            /* Immutable MAC was changed. Display error popup */
            snprintf(popup_text, strlen(result_if_text) + strlen(" MAC Cannot be changed!") + 1,
                "%s MAC cannot be changed!", result_if_text);
        }
        /* Configure popup */
        popup_set_header(
            app->popup,
            popup_header_text,
            1,
            1,
            AlignCenter,
            AlignTop
        );
        popup_set_text(
            app->popup,
            popup_text,
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
    return false;
}

void wendigo_scene_setup_mac_on_exit(void *context) {
    UNUSED(context);
}