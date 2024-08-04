#include "../wendigo_app_i.h"

/* Maximum length of an interface string ("WiFi", "Bluetooth", etc.) */
#define IF_MAX_LEN (10)

/* Local buffer for use of the view */
uint8_t view_bytes[NUM_MAC_BYTES];

/* Strings for popups */
char popup_header_text[IF_MAX_LEN + 11] = "";
char popup_text[IF_MAX_LEN + 50] = "";

/* Convert an array of byteCount uint8_ts into a colon-separated string of bytes.
   strBytes must be initialised with sufficient space to hold the output string.
   For a MAC this is 18 bytes. In general it is 3 * byteCount */
void bytes_to_string(uint8_t* bytes, uint16_t bytesCount, char* strBytes) {
    uint8_t* p_in = bytes;
    const char* hex = "0123456789ABCDEF";
    char* p_out = strBytes;
    for(; p_in < bytes + bytesCount; p_out += 3, ++p_in) {
        p_out[0] = hex[(*p_in >> 4) & 0xF];
        p_out[1] = hex[*p_in & 0xF];
        p_out[2] = ':';
    }
    p_out[-1] = 0;
}

void wendigo_scene_setup_mac_popup_callback(void* context) {
    WendigoApp* app = (WendigoApp*)context;
    scene_manager_previous_scene(app->scene_manager);
}

void wendigo_scene_setup_mac_input_callback(void* context) {
    // If MAC has changed
    //     Popup indicating success or failure
    WendigoApp* app = context;

    /* Did the user change the MAC? */
    if(memcmp(view_bytes, app->interfaces[app->active_interface].mac_bytes, NUM_MAC_BYTES)) {
        char result_if_text[IF_MAX_LEN] = "";
        /* MAC was changed - Update ESP32's MAC */
        /* Also set interface string for popups */
        bool mac_changed = false;
        switch(app->active_interface) {
        case IF_BT_CLASSIC:
        case IF_BLE:
            strcpy(result_if_text, "Bluetooth");
            // TODO: Set bluetooth MAC
            mac_changed = true;
            break;
        case IF_WIFI:
            strcpy(result_if_text, "WiFi");
            // TODO: Set WiFi MAC
            mac_changed = true;
            break;
        case IF_COUNT:
            // Do nothing
            break;
        }
        snprintf(
            popup_header_text,
            strlen("Update  MAC") + strlen(result_if_text) + 1,
            "Update %s MAC",
            result_if_text);
        /* Was the MAC changed successfully? */
        if(mac_changed) {
            /* Record the new MAC in app->interfaces */
            switch(app->active_interface) {
            case IF_BT_CLASSIC:
            case IF_BLE:
                memcpy(app->interfaces[IF_BT_CLASSIC].mac_bytes, view_bytes, NUM_MAC_BYTES);
                memcpy(app->interfaces[IF_BLE].mac_bytes, view_bytes, NUM_MAC_BYTES);
                break;
            case IF_WIFI:
                memcpy(app->interfaces[IF_WIFI].mac_bytes, view_bytes, NUM_MAC_BYTES);
                break;
            default:
                // Do nothing
            }
            snprintf(
                popup_text,
                strlen(result_if_text) + strlen(" MAC Updated!") + 1,
                "%s MAC Updated!",
                result_if_text);
        } else {
            snprintf(
                popup_text,
                strlen(result_if_text) + strlen("Failed to Update  MAC!") + 1,
                "Failed to Update %s MAC!",
                result_if_text);
        }
        /* Configure popup */
        popup_set_header(app->popup, popup_header_text, 64, 3, AlignCenter, AlignTop);
        popup_set_text(app->popup, popup_text, 64, 22, AlignCenter, AlignTop);
        popup_set_icon(app->popup, -1, -1, NULL);
        popup_set_timeout(app->popup, 2000); // 3 secondsn
        popup_enable_timeout(app->popup);
        popup_set_callback(app->popup, wendigo_scene_setup_mac_popup_callback);
        popup_set_context(app->popup, app);

        view_dispatcher_switch_to_view(app->view_dispatcher, WendigoAppViewPopup);
    } else {
        // Should this also be run after the popup?
        scene_manager_handle_back_event(app->scene_manager);
    }
}

void wendigo_scene_setup_mac_changed_callback(void* context) {
    WendigoApp* app = context;
    /* Overwrite the change if the MAC isn't mutable */
    if(!app->interfaces[app->active_interface].mutable) {
        memcpy(view_bytes, app->interfaces[app->active_interface].mac_bytes, NUM_MAC_BYTES);
    }
}

void wendigo_scene_setup_mac_on_enter(void* context) {
    WendigoApp* app = context;
    ByteInput* mac_input = app->setup_mac;

    /* Copy app->mac_bytes into a temp array for modification by the view */
    memcpy(view_bytes, app->interfaces[app->active_interface].mac_bytes, NUM_MAC_BYTES);

    byte_input_set_header_text(mac_input, "MAC Address");
    byte_input_set_result_callback(
        mac_input,
        wendigo_scene_setup_mac_input_callback,
        wendigo_scene_setup_mac_changed_callback,
        app,
        view_bytes,
        NUM_MAC_BYTES);
    view_dispatcher_switch_to_view(app->view_dispatcher, WendigoAppViewSetupMAC);
}

bool wendigo_scene_setup_mac_on_event(void* context, SceneManagerEvent event) {
    //
    UNUSED(context);
    UNUSED(event);
    return false;
}

void wendigo_scene_setup_mac_on_exit(void* context) {
    UNUSED(context);
}
