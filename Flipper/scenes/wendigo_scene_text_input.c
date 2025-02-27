#include "../wendigo_app_i.h"

void wendigo_scene_text_input_callback(void* context) {
    WendigoApp* app = context;

    view_dispatcher_send_custom_event(app->view_dispatcher, Wendigo_EventStartConsole);
}

void wendigo_scene_text_input_on_enter(void* context) {
    WendigoApp* app = context;

    if(false == app->is_custom_tx_string) {
        // Fill text input with selected string so that user can add to it
        size_t length = strlen(app->selected_tx_string);
        furi_assert(length < WENDIGO_TEXT_INPUT_STORE_SIZE);
        bzero(app->text_input_store, WENDIGO_TEXT_INPUT_STORE_SIZE);
        strncpy(app->text_input_store, app->selected_tx_string, length);

        // Add space - because flipper keyboard currently doesn't have a space
        //app->text_input_store[length] = ' ';
        app->text_input_store[length + 1] = '\0';
        app->is_custom_tx_string = true;
    }

    // Setup view
    TextInput* text_input = app->text_input;
    // Add help message to header
    if(0 == strncmp("AT", app->selected_tx_string, strlen("AT"))) {
        app->TERMINAL_MODE = 1;
        text_input_set_header_text(text_input, "Send AT command to UART");
    } else {
        app->TERMINAL_MODE = 0;
        text_input_set_header_text(text_input, "Send command to UART");
    }
    text_input_set_result_callback(
        text_input,
        wendigo_scene_text_input_callback,
        app,
        app->text_input_store,
        WENDIGO_TEXT_INPUT_STORE_SIZE,
        false);

    // TODO: Commenting out for now, review upstream code
    //       from UART_Terminal to determine whether the
    //       function should be ported from the module to
    //       Wendigo.
    //text_input_add_illegal_symbols(text_input);

    view_dispatcher_switch_to_view(app->view_dispatcher, WendigoAppViewTextInput);
}

bool wendigo_scene_text_input_on_event(void* context, SceneManagerEvent event) {
    WendigoApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == Wendigo_EventStartConsole) {
            // Point to custom string to send
            app->selected_tx_string = app->text_input_store;
            scene_manager_next_scene(app->scene_manager, WendigoSceneConsoleOutput);
            consumed = true;
        }
    }

    return consumed;
}

void wendigo_scene_text_input_on_exit(void* context) {
    WendigoApp* app = context;

    text_input_reset(app->text_input);
}
