#include "../wendigo_app_i.h"

// SETUP_MENU_ITEMS defined in wendigo_app_i.h - if you add an entry here, increment it!
static const WendigoItem items[SETUP_CHANNEL_MENU_ITEMS] = {
    {"1", {"On", "Off"}, 2, NO_ACTION, OFF},
    {"2", {"On", "Off"}, 2, NO_ACTION, OFF},
    {"3", {"On", "Off"}, 2, NO_ACTION, OFF},
    {"4", {"On", "Off"}, 2, NO_ACTION, OFF},
    {"5", {"On", "Off"}, 2, NO_ACTION, OFF},
    {"6", {"On", "Off"}, 2, NO_ACTION, OFF},
    {"7", {"On", "Off"}, 2, NO_ACTION, OFF},
    {"8", {"On", "Off"}, 2, NO_ACTION, OFF},
    {"9", {"On", "Off"}, 2, NO_ACTION, OFF},
    {"10", {"On", "Off"}, 2, NO_ACTION, OFF},
    {"11", {"On", "Off"}, 2, NO_ACTION, OFF},
    {"12", {"On", "Off"}, 2, NO_ACTION, OFF},
    {"13", {"On", "Off"}, 2, NO_ACTION, OFF},
};

static void wendigo_scene_setup_channel_var_list_enter_callback(void* context, uint32_t index) {
    furi_assert(context);
    WendigoApp* app = context;

    furi_assert(index < SETUP_MENU_ITEMS);
    const WendigoItem* item = &items[index];

    const int selected_option_index = app->setup_selected_option_index[index];
    furi_assert(selected_option_index < item->num_options_menu);
    app->setup_selected_menu_index = index;

    // TODO: if index == SETUP_MAC_IDX
    //          display view
    //          if selected_option_index == OPTION_MAC_SET 
    //              // Is there a way to make it mutable/immutable?
    // if index == SETUP_CHANNEL_IDX
    //      if selected_option_index == OPTION_CHANNEL_SELECTED
    //          // start scene_setup_channel
}

static void wendigo_scene_setup_channel_var_list_change_callback(VariableItem* item) {
    furi_assert(item);

    WendigoApp* app = variable_item_get_context(item);
    furi_assert(app);

    const WendigoItem* menu_item = &items[app->setup_selected_menu_index];
    uint8_t item_index = variable_item_get_current_value_index(item);
    furi_assert(item_index < menu_item->num_options_menu);
    variable_item_set_current_value_text(item, menu_item->options_menu[item_index]);
    app->setup_selected_option_index[app->setup_selected_menu_index] = item_index;

    // UART Pins
    if(app->setup_selected_menu_index == UART_PINS_ITEM_IDX) {
        app->new_uart_ch = item_index > 0;
    }

    // BaudRate
    if(app->setup_selected_menu_index == BAUDRATE_ITEM_IDX) {
        app->NEW_BAUDRATE = atoi(menu_item->options_menu[item_index]);
    }

    // HEX mode
    if(app->setup_selected_menu_index == HEX_MODE_ITEM_IDX) {
        bool new_mode = item_index > 0;
        if(app->hex_mode != new_mode) {
            app->hex_mode = new_mode;
            app->text_input_store[0] = '\0';
            app->selected_menu_index = 0;
            for(int i = 0; i < START_MENU_ITEMS; ++i) {
                app->selected_option_index[i] = 0;
            }
        }
    }
}

void wendigo_scene_setup_channel_on_enter(void* context) {
    WendigoApp* app = context;
    VariableItemList* var_item_list = app->var_item_list;

    variable_item_list_set_enter_callback(
        var_item_list, wendigo_scene_setup_channel_var_list_enter_callback, app);

    VariableItem* item;
    for(int i = 0; i < SETUP_MENU_ITEMS; ++i) {
        item = variable_item_list_add(
            var_item_list,
            items[i].item_string,
            items[i].num_options_menu,
            wendigo_scene_setup_channel_var_list_change_callback,
            app);
        variable_item_set_current_value_index(item, app->setup_selected_option_index[i]);
        variable_item_set_current_value_text(
            item, items[i].options_menu[app->setup_selected_option_index[i]]);
    }

    variable_item_list_set_selected_item(
        var_item_list, scene_manager_get_scene_state(app->scene_manager, WendigoSceneSetup));

    view_dispatcher_switch_to_view(app->view_dispatcher, WendigoAppViewVarItemList);
}

bool wendigo_scene_setup_channel_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    WendigoApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
    } else if(event.type == SceneManagerEventTypeTick) {
        app->setup_selected_menu_index =
            variable_item_list_get_selected_item_index(app->var_item_list);
        consumed = true;
    }

    return consumed;
}

void wendigo_scene_setup_channel_on_exit(void* context) {
    WendigoApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
