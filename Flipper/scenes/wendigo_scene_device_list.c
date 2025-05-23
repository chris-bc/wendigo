#include "../wendigo_app_i.h"

static void wendigo_scene_device_list_var_list_enter_callback(void* context, uint32_t index) {
    furi_assert(context);
    WendigoApp* app = context;

    furi_assert(index < bt_devices_count);
    // TODO: Use the same scene for device_list and tagged_devices
    const flipper_bt_device *item = bt_devices[index];
    UNUSED(item);

    app->device_list_selected_menu_index = index;

    // TODO: Display a new "device info" scene

}

static void wendigo_scene_device_list_var_list_change_callback(VariableItem* item) {
    furi_assert(item);

    WendigoApp* app = variable_item_get_context(item);
    furi_assert(app);

    const flipper_bt_device *menu_item = items[app->device_list_selected_menu_index];
    uint8_t item_index = variable_item_get_current_value_index(item);
    furi_assert(item_index < bt_devices_count);
    // TODO: The following will be useful for managing tag options (on/off)
    // variable_item_set_current_value_text(item, menu_item->options_menu[item_index]);
    // app->setup_selected_option_index[app->setup_selected_menu_index] = item_index;

}

void wendigo_scene_device_list_on_enter(void* context) {
    WendigoApp* app = context;
    VariableItemList* var_item_list = app->devices_var_item_list;

    variable_item_list_set_enter_callback(
        var_item_list, wendigo_scene_device_list_var_list_enter_callback, app);

    variable_item_list_reset(var_item_list);
    // TODO: Update this to support either all or tagged devices
    VariableItem* item;
    char *bda_str;
    for(int i = 0; i < bt_devices_count; ++i) {
        bda_str = malloc(sizeof(char) * (MAC_STRLEN + 1));
        if (bda_str == NULL) {
            // TODO: Panic
            break; // TODO: I wonder whether this will drop out of the for loop entirely...
        }
        bytes_to_string(bt_devices[i]->dev.bda, MAC_BYTES, bda_str);
        item = variable_item_list_add(
            var_item_list,
            bda_str,
            1, // TODO: Change this for menu items with options (although I don't think there are any?)
            wendigo_scene_device_list_var_list_change_callback,
            app);
        // TODO: Should I free bda_str now? If I have to free it later it's going to be a right pain :/
        // TODO: Support device_list_selected_option_index and tagged_devices_selected_option_index
        /* Actually I don't think any menu items will have options
        variable_item_set_current_value_index(item, app->device_list_selected_option_index[i]);
        variable_item_set_current_value_text(
            item, items[i].options_menu[app->device_list_selected_option_index[i]]); */
    }

    variable_item_list_set_selected_item(
        var_item_list, scene_manager_get_scene_state(app->scene_manager, WendigoSceneDeviceList));

    view_dispatcher_switch_to_view(app->view_dispatcher, WendigoAppViewDeviceList);
}

bool wendigo_scene_device_list_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    WendigoApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == Wendigo_EventSetup) {
            /* Save scene state */
            scene_manager_set_scene_state(
                app->scene_manager, WendigoSceneDeviceList, app->device_list_selected_menu_index);
            scene_manager_next_scene(app->scene_manager, WendigoSceneSetupChannel);
        } else if(event.event == Wendigo_EventMAC) {
            scene_manager_set_scene_state(
                app->scene_manager, WendigoSceneDeviceList, app->device_list_selected_menu_index);
            scene_manager_next_scene(app->scene_manager, WendigoSceneSetupMAC);
        }
        consumed = true;
    } else if(event.type == SceneManagerEventTypeTick) {
        app->device_list_selected_menu_index =
            variable_item_list_get_selected_item_index(app->devices_var_item_list);
        consumed = true;
    }

    return consumed;
}

void wendigo_scene_device_list_on_exit(void* context) {
    WendigoApp* app = context;
    variable_item_list_reset(app->devices_var_item_list);
}
