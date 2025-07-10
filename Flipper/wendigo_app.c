#include "wendigo_app_i.h"
#include "wendigo_scan.h"

#include <furi.h>
#include <furi_hal.h>
#include <expansion/expansion.h>
#include <math.h>

/* UART rx callback for Console Output scene */
extern void wendigo_console_output_handle_rx_data_cb(uint8_t* buf, size_t len, void* context);

static bool wendigo_app_custom_event_callback(void* context, uint32_t event) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_app_custom_event_callback()");
    furi_assert(context);
    WendigoApp* app = context;
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_app_custom_event_callback()");
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool wendigo_app_back_event_callback(void* context) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_app_back_event_callback()");
    furi_assert(context);
    WendigoApp* app = context;
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_app_back_event_callback()");
    return scene_manager_handle_back_event(app->scene_manager);
}

static void wendigo_app_tick_event_callback(void* context) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_app_tick_event_callback()");
    furi_assert(context);
    WendigoApp* app = context;
    if (app->is_scanning) {
        /* Is it time to poll ESP32 to ensure it's still scanning? */
        uint32_t now = furi_hal_rtc_get_timestamp();
        if (now - app->last_packet > ESP32_POLL_INTERVAL) {
            wendigo_set_scanning_active(app, true);
        }
    }
    scene_manager_handle_tick_event(app->scene_manager);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_app_tick_event_callback()");
}

/* Generic handler for app->popup that restores the previous view */
void wendigo_popup_callback(void *context) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_popup_callback()");
    WendigoApp *app = (WendigoApp *)context;
    bool done = scene_manager_previous_scene(app->scene_manager);
    if (!done) {
        /* No previous scene - Start the main menu scene */
        // TODO: Alongside wendigo_display_popup() (below), restore the scene that was actually running prior to the popup
        scene_manager_next_scene(app->scene_manager, WendigoSceneStart);
    }
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_popup_callback()");
}

void wendigo_display_popup(WendigoApp *app, char *header, char *body) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_display_popup()");
    // TODO: Review and kill this
    char * newBody = malloc(sizeof(char*) * (strlen(body) + 1));
    strncpy(newBody, body, strlen(body) + 1);
    popup_set_header(app->popup, header, 64, 3, AlignCenter, AlignTop);
    popup_set_text(app->popup, newBody, 64, 22, AlignCenter, AlignTop);
    popup_set_icon(app->popup, -1, -1, NULL); // TODO: Find a fun icon to use
    popup_set_timeout(app->popup, 5000); // was 2000
    popup_enable_timeout(app->popup);
    popup_set_callback(app->popup, wendigo_popup_callback);
    popup_set_context(app->popup, app);
    // TODO: Check which scene is active so we can restore it later. For now assuming we're on the main menu.
    scene_manager_set_scene_state(app->scene_manager, WendigoSceneStart, app->selected_menu_index);
    view_dispatcher_switch_to_view(app->view_dispatcher, WendigoAppViewPopup);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_display_popup()");
}

/* Initialise app->interfaces - Default all radios to on */
void wendigo_interface_init(WendigoApp* app) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_interface_init()");
    for(int i = 0; i < IF_COUNT; ++i) {
        app->interfaces[i].active = true;
        app->interfaces[i].mutable = true;
    }
    // TODO: Retrieve actual MAC
    const uint8_t mac_wifi[MAC_BYTES] = {0xa6, 0xe0, 0x57, 0x4f, 0x57, 0xac};
    const uint8_t mac_bt[MAC_BYTES] = {0xa6, 0xe0, 0x57, 0x4f, 0x57, 0xaf};
    memcpy(app->interfaces[IF_WIFI].mac_bytes, mac_wifi, MAC_BYTES);
    memcpy(app->interfaces[IF_BT_CLASSIC].mac_bytes, mac_bt, MAC_BYTES);
    memcpy(app->interfaces[IF_BLE].mac_bytes, mac_bt, MAC_BYTES);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_interface_init()");
}

WendigoApp* wendigo_app_alloc() {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_app_alloc()");
    WendigoApp* app = malloc(sizeof(WendigoApp));

    app->gui = furi_record_open(RECORD_GUI);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&wendigo_scene_handlers, app);
    view_dispatcher_enable_queue(app->view_dispatcher);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, wendigo_app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, wendigo_app_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, wendigo_app_tick_event_callback, 100);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        WendigoAppViewVarItemList,
        variable_item_list_get_view(app->var_item_list));

    for(int i = 0; i < START_MENU_ITEMS; ++i) {
        app->selected_option_index[i] = 0;
    }

    for(int i = 0; i < SETUP_MENU_ITEMS; ++i) {
        app->setup_selected_option_index[i] = 0;
    }

    /* Initialise the channel bitmasks - pow() is slow so hardcode 0 & 1 and use multiplication for the rest */
    app->CH_MASK[0] = 0;
    app->CH_MASK[1] = 1;
    for(int i = 2; i <= SETUP_CHANNEL_MENU_ITEMS; ++i) {
        app->CH_MASK[i] = app->CH_MASK[i - 1] * 2;
    }

    /* Default to enabling all channels */
    for(int i = 0; i <= SETUP_CHANNEL_MENU_ITEMS; ++i) {
        /* Bitwise - Add current channel to app->channel_mask */
        app->channel_mask = app->channel_mask | app->CH_MASK[i];
    }
    /* Also set CH_MASK_ALL */
    app->channel_mask = app->channel_mask | CH_MASK_ALL;

    /* Initialise and enable all interfaces */
    wendigo_interface_init(app);
    app->is_scanning = false;

    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, WendigoAppViewHelp, widget_get_view(app->widget));

    app->text_box = text_box_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, WendigoAppViewConsoleOutput, text_box_get_view(app->text_box));
    app->text_box_store = furi_string_alloc();
    furi_string_reserve(app->text_box_store, WENDIGO_TEXT_BOX_STORE_SIZE);

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, WendigoAppViewTextInput, text_input_get_view(app->text_input));

    // TODO: Ditch the hex input
    app->hex_input = wendigo_hex_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, WendigoAppViewHexInput, wendigo_hex_input_get_view(app->hex_input));

    /* Initialise MAC address view */
    app->setup_mac = byte_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, WendigoAppViewSetupMAC, byte_input_get_view(app->setup_mac));

    /* Initialise the popup */
    app->popup = popup_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, WendigoAppViewPopup, popup_get_view(app->popup));
    
    /* Device list (all and tagged) */
    app->devices_var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(app->view_dispatcher, WendigoAppViewDeviceList,
        variable_item_list_get_view(app->devices_var_item_list));
    
    /* Initialise the last packet received time */
    app->last_packet = furi_hal_rtc_get_timestamp();

    scene_manager_next_scene(app->scene_manager, WendigoSceneStart);

    FURI_LOG_T(WENDIGO_TAG, "End wendigo_app_alloc()");
    return app;
}

void wendigo_app_free(WendigoApp* app) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_app_free()");
    furi_assert(app);

    // Views
    view_dispatcher_remove_view(app->view_dispatcher, WendigoAppViewVarItemList);
    view_dispatcher_remove_view(app->view_dispatcher, WendigoAppViewHelp);
    view_dispatcher_remove_view(app->view_dispatcher, WendigoAppViewConsoleOutput);
    view_dispatcher_remove_view(app->view_dispatcher, WendigoAppViewTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, WendigoAppViewHexInput);
    view_dispatcher_remove_view(app->view_dispatcher, WendigoAppViewSetupMAC);
    view_dispatcher_remove_view(app->view_dispatcher, WendigoAppViewPopup);
    view_dispatcher_remove_view(app->view_dispatcher, WendigoAppViewDeviceList);

    variable_item_list_free(app->var_item_list);
    widget_free(app->widget);
    text_box_free(app->text_box);
    furi_string_free(app->text_box_store);
    text_input_free(app->text_input);
    wendigo_hex_input_free(app->hex_input);
    byte_input_free(app->setup_mac);
    popup_free(app->popup);
    variable_item_list_free(app->devices_var_item_list);

    // View dispatcher
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    /* Free device cache and UART buffer */
    wendigo_free_uart_buffer();
    wendigo_free_devices();

    wendigo_uart_free(app->uart);

    // Close records
    furi_record_close(RECORD_GUI);

    free(app);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_app_free()");
}

int32_t wendigo_app(void* p) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_app()");
    UNUSED(p);
    // Disable expansion protocol to avoid interference with UART Handle
    Expansion* expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(expansion);

    WendigoApp* wendigo_app = wendigo_app_alloc();

    wendigo_app->uart = wendigo_uart_init(wendigo_app);
    /* Set UART callback using wendigo_scan */
    wendigo_uart_set_binary_cb(wendigo_app->uart);

    view_dispatcher_run(wendigo_app->view_dispatcher);

    wendigo_app_free(wendigo_app);

    // Return previous state of expansion
    expansion_enable(expansion);
    furi_record_close(RECORD_EXPANSION);

    FURI_LOG_T(WENDIGO_TAG, "End wendigo_app()");
    return 0;
}

void wendigo_uart_set_binary_cb(Wendigo_Uart *uart) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_uart_set_binary_cb()");
    wendigo_uart_set_handle_rx_data_cb(uart, wendigo_scan_handle_rx_data_cb);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_uart_set_binary_cb()");
}

void wendigo_uart_set_console_cb(Wendigo_Uart *uart) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_uart_set_console_cb()");
    wendigo_uart_set_handle_rx_data_cb(uart, wendigo_console_output_handle_rx_data_cb);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_uart_set_console_cb()");
}

/* Convert an array of bytesCount uint8_ts into a colon-separated string of bytes.
   strBytes must be initialised with sufficient space to hold the output string.
   For a MAC this is 18 bytes. In general it is 3 * byteCount */
void bytes_to_string(uint8_t *bytes, uint16_t bytesCount, char *strBytes) {
    FURI_LOG_T(WENDIGO_TAG, "Start bytes_to_string()");
    uint8_t *p_in = bytes;
    const char *hex = "0123456789ABCDEF";
    char *p_out = strBytes;
    for(; p_in < bytes + bytesCount; p_out += 3, ++p_in) {
        p_out[0] = hex[(*p_in >> 4) & 0xF];
        p_out[1] = hex[*p_in & 0xF];
        p_out[2] = ':';
    }
    p_out[-1] = 0;
    FURI_LOG_T(WENDIGO_TAG, "End bytes_to_string()");
}
