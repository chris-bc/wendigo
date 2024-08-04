#include "../wendigo_app_i.h"

// SETUP_MENU_ITEMS defined in wendigo_app_i.h - if you add an entry here, increment it!
static const WendigoItem items[SETUP_MENU_ITEMS] = {
    {"BLE", {"On", "Off", "MAC"}, 3, NO_ACTION, OFF},
    {"BT Classic", {"On", "Off", "MAC"}, 3, NO_ACTION, OFF},
    {"WiFi", {"On", "Off", "MAC"}, 3, NO_ACTION, OFF},
    {"Channel", {"All", "Selected"}, 2, OPEN_SETUP, OFF},
    // YAGNI: Remove mode_mask from the data model
};

#define CH_ALL      (0)
#define CH_SELECTED (1)
#define RADIO_ON    (0)
#define RADIO_OFF   (1)
#define RADIO_MAC   (2)

static void wendigo_scene_setup_var_list_enter_callback(void* context, uint32_t index) {
    furi_assert(context);
    WendigoApp* app = context;

    furi_assert(index < SETUP_MENU_ITEMS);
    const WendigoItem* item = &items[index];

    const int selected_option_index = app->setup_selected_option_index[index];
    furi_assert(selected_option_index < item->num_options_menu);
    app->setup_selected_menu_index = index;

    switch(item->action) {
    case OPEN_SETUP:
        // TODO: If value is "Selected" display channels view
        //       Otherwise select all channels
        if(selected_option_index == CH_SELECTED) {
            /* Display channel selection view */
            view_dispatcher_send_custom_event(app->view_dispatcher, Wendigo_EventSetup);
        }
        break;
    default:
        /* Note: Additional check required here if additional menu items are added with 3 or more options.
                     At the moment we can assume that if selected option is RADIO_MAC we're displaying a MAC,
                     that may not always be the case
            */
        // TODO: enabled/disable BT, BLE, WiFi

        if(selected_option_index == RADIO_MAC) {
            // Configure byte_input's value and mutability based on item->item_string
            uint8_t mac[NUM_MAC_BYTES] = {0xa6, 0xe0, 0x57, 0x4f, 0x57, 0xac};
            if(!(strncmp(item->item_string, "BLE", 3) && strncmp(item->item_string, "BT", 2))) {
                // Display (immutable?) bluetooth MAC
                // TODO: Fetch MAC
                memcpy(app->mac_bytes, mac, NUM_MAC_BYTES);
                app->mac_interface = IF_BLUETOOTH;
            } else if(!strncmp(item->item_string, "WiFi", 4)) {
                // Display mutable WiFi MAC
                // TODO: Fetch MAC
                memcpy(app->mac_bytes, mac, NUM_MAC_BYTES);
                app->mac_interface = IF_WIFI;
            } else {
                // TODO: Panic
            }
            view_dispatcher_send_custom_event(app->view_dispatcher, Wendigo_EventMAC);
        }
        break;
    }
}

static void wendigo_scene_setup_var_list_change_callback(VariableItem* item) {
    furi_assert(item);

    WendigoApp* app = variable_item_get_context(item);
    furi_assert(app);

    const WendigoItem* menu_item = &items[app->setup_selected_menu_index];
    uint8_t item_index = variable_item_get_current_value_index(item);
    furi_assert(item_index < menu_item->num_options_menu);
    variable_item_set_current_value_text(item, menu_item->options_menu[item_index]);
    app->setup_selected_option_index[app->setup_selected_menu_index] = item_index;

    /* Handle moving between "all" and "selected" channels */
    if(menu_item->action == OPEN_SETUP) {
        if(item_index == CH_ALL) {
            app->channel_mask |= CH_MASK_ALL;
        } else if(item_index == CH_SELECTED) {
            /* Turn off the "all" bitmask if it is set */
            if((app->channel_mask & CH_MASK_ALL) == CH_MASK_ALL) {
                app->channel_mask -= CH_MASK_ALL;
            }
        } else {
            // TODO: Panic
        }
    }
}

void wendigo_scene_setup_on_enter(void* context) {
    WendigoApp* app = context;
    VariableItemList* var_item_list = app->var_item_list;

    variable_item_list_set_enter_callback(
        var_item_list, wendigo_scene_setup_var_list_enter_callback, app);

    variable_item_list_reset(var_item_list);
    VariableItem* item;
    for(int i = 0; i < SETUP_MENU_ITEMS; ++i) {
        item = variable_item_list_add(
            var_item_list,
            items[i].item_string,
            items[i].num_options_menu,
            wendigo_scene_setup_var_list_change_callback,
            app);
        variable_item_set_current_value_index(item, app->setup_selected_option_index[i]);
        variable_item_set_current_value_text(
            item, items[i].options_menu[app->setup_selected_option_index[i]]);
    }

    variable_item_list_set_selected_item(
        var_item_list, scene_manager_get_scene_state(app->scene_manager, WendigoSceneSetup));

    view_dispatcher_switch_to_view(app->view_dispatcher, WendigoAppViewVarItemList);
}

bool wendigo_scene_setup_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    WendigoApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == Wendigo_EventSetup) {
            /* Save scene state */
            scene_manager_set_scene_state(
                app->scene_manager, WendigoSceneSetup, app->setup_selected_menu_index);
            scene_manager_next_scene(app->scene_manager, WendigoSceneSetupChannel);
        } else if(event.event == Wendigo_EventMAC) {
            scene_manager_set_scene_state(
                app->scene_manager, WendigoSceneSetup, app->setup_selected_menu_index);
            scene_manager_next_scene(app->scene_manager, WendigoSceneSetupMAC);
        }
        consumed = true;
    } else if(event.type == SceneManagerEventTypeTick) {
        app->setup_selected_menu_index =
            variable_item_list_get_selected_item_index(app->var_item_list);
        consumed = true;
    }

    return consumed;
}

void wendigo_scene_setup_on_exit(void* context) {
    WendigoApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
