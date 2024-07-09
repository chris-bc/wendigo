#include "wendigo_app_i.h"

#include <furi.h>
#include <furi_hal.h>
#include <expansion/expansion.h>

static const uint16_t CH_MASK[SETUP_CHANNEL_MENU_ITEMS + 1] = {0, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096};

static bool wendigo_app_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    WendigoApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool wendigo_app_back_event_callback(void* context) {
    furi_assert(context);
    WendigoApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void wendigo_app_tick_event_callback(void* context) {
    furi_assert(context);
    WendigoApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

WendigoApp* wendigo_app_alloc() {
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

    /* Default to enabling all channels */
    for (int i = 0; i <= SETUP_CHANNEL_MENU_ITEMS; ++i) {
        /* Bitwise - Add current channel to app->channel_mask */
        app->channel_mask = app->channel_mask | CH_MASK[i];
    }

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
        app->view_dispatcher,
        WendigoAppViewHexInput,
        wendigo_hex_input_get_view(app->hex_input));
    
    /* Initialise MAC address view */
    app->setup_mac = byte_input_alloc();
    view_dispatcher_add_view(app->view_dispatcher, WendigoAppViewSetupMAC, byte_input_get_view(app->setup_mac));

    app->setup_selected_option_index[BAUDRATE_ITEM_IDX] = DEFAULT_BAUDRATE_OPT_IDX;

    scene_manager_next_scene(app->scene_manager, WendigoSceneStart);

    return app;
}

void wendigo_app_free(WendigoApp* app) {
    furi_assert(app);

    // Views
    view_dispatcher_remove_view(app->view_dispatcher, WendigoAppViewVarItemList);
    view_dispatcher_remove_view(app->view_dispatcher, WendigoAppViewHelp);
    view_dispatcher_remove_view(app->view_dispatcher, WendigoAppViewConsoleOutput);
    view_dispatcher_remove_view(app->view_dispatcher, WendigoAppViewTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, WendigoAppViewHexInput);
    view_dispatcher_remove_view(app->view_dispatcher, WendigoAppViewSetupMAC);

    variable_item_list_free(app->var_item_list);
    widget_free(app->widget);
    text_box_free(app->text_box);
    furi_string_free(app->text_box_store);
    text_input_free(app->text_input);
    wendigo_hex_input_free(app->hex_input);
    byte_input_free(app->setup_mac);

    // View dispatcher
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    wendigo_uart_free(app->uart);

    // Close records
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t wendigo_app(void* p) {
    UNUSED(p);
    // Disable expansion protocol to avoid interference with UART Handle
    Expansion* expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(expansion);

    WendigoApp* wendigo_app = wendigo_app_alloc();

    wendigo_app->uart = wendigo_uart_init(wendigo_app);

    view_dispatcher_run(wendigo_app->view_dispatcher);

    wendigo_app_free(wendigo_app);

    // Return previous state of expansion
    expansion_enable(expansion);
    furi_record_close(RECORD_EXPANSION);

    return 0;
}
