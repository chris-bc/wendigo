#include "../wendigo_app_i.h"

/** Maximum length of an interface string ("WiFi", "Bluetooth", etc.) */
#define IF_MAX_LEN (10)

/** Local buffer for use of the view */
uint8_t view_bytes[MAC_BYTES];

/** Strings for popups */
char popup_header_text[IF_MAX_LEN + 11] = "";
char popup_text[IF_MAX_LEN + 50] = "";

void wendigo_scene_setup_mac_popup_callback(void *context) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_setup_mac_popup_callback()");
    WendigoApp *app = (WendigoApp*)context;
    scene_manager_previous_scene(app->scene_manager);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_setup_mac_popup_callback()");
}

void wendigo_scene_setup_mac_input_callback(void *context) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_setup_mac_input_callback()");
    /* If MAC has changed
     * Popup indicating success or failure */
    WendigoApp *app = context;

    /* Did the user change the MAC? */
    if (memcmp(view_bytes, app->interfaces[app->active_interface].mac_bytes, MAC_BYTES)) {
        char result_if_text[IF_MAX_LEN] = "";
        /* MAC was changed - Update ESP32's MAC */
        /* Also set interface string for popups */
        bool mac_changed = false;
        switch (app->active_interface) {
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
        snprintf(popup_header_text,
            strlen("Update  MAC") + strlen(result_if_text) + 1,
            "Update %s MAC", result_if_text);
        /* Was the MAC changed successfully? */
        if (mac_changed) {
            /* Record the new MAC in app->interfaces */
            switch (app->active_interface) {
                case IF_BT_CLASSIC:
                case IF_BLE:
                    memcpy(app->interfaces[IF_BT_CLASSIC].mac_bytes, view_bytes, MAC_BYTES);
                    memcpy(app->interfaces[IF_BLE].mac_bytes, view_bytes, MAC_BYTES);
                    break;
                case IF_WIFI:
                    memcpy(app->interfaces[IF_WIFI].mac_bytes, view_bytes, MAC_BYTES);
                    break;
                default:
                    /* Do nothing */
            }
            snprintf(popup_text,
                strlen(result_if_text) + strlen(" MAC Updated!") + 1,
                "%s MAC Updated!", result_if_text);
        } else {
            snprintf(popup_text,
                strlen(result_if_text) + strlen("Failed to Update  MAC!") + 1,
                "Failed to Update %s MAC!", result_if_text);
        }
        wendigo_display_popup(app, popup_header_text, popup_text);
    } else {
        // TODO: Should this also be run after the popup?
        scene_manager_handle_back_event(app->scene_manager);
    }
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_setup_mac_input_callback()");
}

void wendigo_scene_setup_mac_changed_callback(void *context) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_setup_mac_changed_callback()");
    WendigoApp *app = context;
    /* Overwrite the change if the MAC isn't mutable */
    if (!app->interfaces[app->active_interface].mutable) {
        memcpy(view_bytes, app->interfaces[app->active_interface].mac_bytes, MAC_BYTES);
    }
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_setup_mac_changed_callback()");
}

void wendigo_scene_setup_mac_on_enter(void *context) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_setup_mac_on_enter()");
    WendigoApp *app = context;
    ByteInput *mac_input = app->setup_mac;
    app->current_view = WendigoAppViewSetupMAC;

    /* Copy app->mac_bytes into a temp array for modification by the view */
    memcpy(view_bytes, app->interfaces[app->active_interface].mac_bytes, MAC_BYTES);

    byte_input_set_header_text(mac_input, "MAC Address");
    byte_input_set_result_callback(mac_input,
        wendigo_scene_setup_mac_input_callback,
        wendigo_scene_setup_mac_changed_callback, app, view_bytes, MAC_BYTES);
    view_dispatcher_switch_to_view(app->view_dispatcher, WendigoAppViewSetupMAC);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_setup_mac_on_enter()");
}

bool wendigo_scene_setup_mac_on_event(void *context, SceneManagerEvent event) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_setup_mac_on_event()");
    //
    UNUSED(context);
    UNUSED(event);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_setup_mac_on_event()");
    return false;
}

void wendigo_scene_setup_mac_on_exit(void *context) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_setup_mac_on_exit()");
    UNUSED(context);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_setup_mac_on_exit()");
}
