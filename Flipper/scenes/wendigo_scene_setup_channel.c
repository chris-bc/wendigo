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

static const uint8_t CH_ON = 0;
static const uint8_t CH_OFF = 1;

/* I'm not a fan of UART Terminal recent design decisions - I should have based
   this project off Gravity rather than it.
   Instead of adding yet another application variable to track selected menu item
   use a static variable here */
static uint8_t selected_menu_index = 0;

/* Y'know ... I don't think I need this function to do anything at all
   Channel selection is handled by list_changed callback */
static void wendigo_scene_setup_channel_var_list_enter_callback(void* context, uint32_t index) {
    furi_assert(context);
    UNUSED(index);
}

static void wendigo_scene_setup_channel_var_list_change_callback(VariableItem* item) {
    furi_assert(item);

    WendigoApp* app = variable_item_get_context(item);
    furi_assert(app);

    selected_menu_index = variable_item_list_get_selected_item_index(app->var_item_list);
    furi_assert(selected_menu_index < SETUP_CHANNEL_MENU_ITEMS);

    const WendigoItem* menu_item = &items[selected_menu_index];
    uint8_t item_index = variable_item_get_current_value_index(item);
    furi_assert(item_index < menu_item->num_options_menu);
    variable_item_set_current_value_text(item, menu_item->options_menu[item_index]);

    /* Store selected channel state */
    if(item_index == CH_ON) {
        app->channel_mask = app->channel_mask | app->CH_MASK[selected_menu_index + 1];
    } else {
        /* Eck. This got less elegant.
           If channel currently ON, disable it by subtracting that channel's bit value */
        if((app->channel_mask & app->CH_MASK[selected_menu_index + 1]) ==
           app->CH_MASK[selected_menu_index + 1]) {
            app->channel_mask -= app->CH_MASK[selected_menu_index + 1];
        }
    }
}

void wendigo_scene_setup_channel_on_enter(void* context) {
    WendigoApp* app = context;
    VariableItemList* var_item_list = app->var_item_list;

    variable_item_list_set_enter_callback(
        var_item_list, wendigo_scene_setup_channel_var_list_enter_callback, app);

    VariableItem* item;
    for(int i = 0; i < SETUP_CHANNEL_MENU_ITEMS; ++i) {
        item = variable_item_list_add(
            var_item_list,
            items[i].item_string,
            items[i].num_options_menu,
            wendigo_scene_setup_channel_var_list_change_callback,
            app);

        /* Use channels array and bitmask to determine current option value
           I'd rather use the ternary operator but this is more readable and I think they'll be
           optimised down to the same code */
        uint16_t ch_value;
        if((app->channel_mask & app->CH_MASK[i + 1]) == app->CH_MASK[i + 1]) {
            ch_value = CH_ON;
        } else {
            ch_value = CH_OFF;
        }
        variable_item_set_current_value_index(item, ch_value);
        variable_item_set_current_value_text(item, items[i].options_menu[ch_value]);
    }
    variable_item_list_set_selected_item(var_item_list, 0);
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
