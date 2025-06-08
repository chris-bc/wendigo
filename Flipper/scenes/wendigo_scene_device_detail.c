#include "../wendigo_app_i.h"
#include "../wendigo_scan.h"

flipper_bt_device *device = NULL;

void wendigo_scene_device_detail_set_device(flipper_bt_device *d) {
    device = d;
}

static void wendigo_scene_device_detail_var_list_enter_callback(void* context, uint32_t index) {
    furi_assert(context);
    WendigoApp* app = context;

//    furi_assert(index < ((display_selected_only) ? bt_selected_devices_count : bt_devices_count));

    UNUSED(app); UNUSED(index);

    // TODO: something

}

static void wendigo_scene_device_detail_var_list_change_callback(VariableItem* item) {
    furi_assert(item);

    WendigoApp* app = variable_item_get_context(item);
    furi_assert(app);

    uint8_t item_index = variable_item_get_current_value_index(item);
    UNUSED(item_index);
//    furi_assert(item_index < bt_devices_count);
    // TODO: The following will be useful for managing tag options (on/off)
    // variable_item_set_current_value_text(item, menu_item->options_menu[item_index]);
    // app->setup_selected_option_index[app->setup_selected_menu_index] = item_index;

}

void wendigo_scene_device_detail_on_enter(void* context) {
    WendigoApp* app = context;
    VariableItemList* var_item_list = app->detail_var_item_list;
    app->current_view = WendigoAppViewDeviceDetail;

    variable_item_list_set_enter_callback(
        var_item_list, wendigo_scene_device_detail_var_list_enter_callback, app);

    variable_item_list_reset(var_item_list);
    VariableItem* item;
    // TODO: Populate with device details
    item = variable_item_list_add(
            var_item_list,
            "foo",
            1,
            wendigo_scene_device_detail_var_list_change_callback,
            app);
    UNUSED(item);

    variable_item_list_set_selected_item(
        var_item_list, scene_manager_get_scene_state(app->scene_manager, WendigoSceneDeviceDetail));

    view_dispatcher_switch_to_view(app->view_dispatcher, WendigoAppViewDeviceDetail);
}

bool wendigo_scene_device_detail_on_event(void* context, SceneManagerEvent event) {
    WendigoApp* app = context;
    bool consumed = false;
    UNUSED(app);

    if(event.type == SceneManagerEventTypeCustom) {
        switch (event.event) {
            case Wendigo_EventListDevices:
                // scene_manager_set_scene_state(
                //     app->scene_manager, WendigoSceneDeviceDetail, app->device_detail_selected_menu_index);
                //scene_manager_next_scene(app->scene_manager, WendigoSceneDeviceDetail);
                break;
            default:
                // TODO: Panic
                break;
        }
        consumed = true;
    } else if(event.type == SceneManagerEventTypeTick) {
        // app->device_detail_selected_menu_index =
        //     variable_item_list_get_selected_item_index(app->detail_var_item_list);
        consumed = true;
    }
    return consumed;
}

void wendigo_scene_device_detail_on_exit(void* context) {
    WendigoApp* app = context;
    variable_item_list_reset(app->detail_var_item_list);
}
