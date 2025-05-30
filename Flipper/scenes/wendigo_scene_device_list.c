#include "../wendigo_app_i.h"
#include "../wendigo_scan.h"

/* Public method from wendigo_scene_device_detail.c */
extern void wendigo_scene_device_detail_set_device(flipper_bt_device *d);

/* This scene is used to both display all devices and display selected devices */
bool display_selected_only = false;

static void wendigo_scene_device_list_var_list_enter_callback(void* context, uint32_t index) {
    furi_assert(context);
    WendigoApp* app = context;

    furi_assert(index < ((display_selected_only) ? bt_selected_devices_count : bt_devices_count));

    flipper_bt_device *item = (display_selected_only) ? bt_selected_devices[index] : bt_devices[index];

    app->device_list_selected_menu_index = index;

    /* Display details for `item` */
    wendigo_scene_device_detail_set_device(item);
    view_dispatcher_send_custom_event(app->view_dispatcher, Wendigo_EventListDevices);
}

// TODO: Consider removing this entirely
static void wendigo_scene_device_list_var_list_change_callback(VariableItem* item) {
    furi_assert(item);

    WendigoApp* app = variable_item_get_context(item);
    furi_assert(app);

    const flipper_bt_device *menu_item = bt_devices[app->device_list_selected_menu_index];
    UNUSED(menu_item);
    uint8_t item_index = variable_item_get_current_value_index(item);
    furi_assert(item_index < bt_devices_count);
    // TODO: The following will be useful for managing tag options (on/off)
    // variable_item_set_current_value_text(item, menu_item->options_menu[item_index]);
    // app->setup_selected_option_index[app->setup_selected_menu_index] = item_index;

}

void wendigo_scene_device_list_update(WendigoApp *app, flipper_bt_device *dev) {
    char *name;
    if (dev->dev.bdname_len == 0 || dev->dev.bdname == NULL) {
        name = malloc(sizeof(char) * (MAC_STRLEN + 1));
        if (name == NULL) {
            // TODO: Panic
            return;
        }
        bytes_to_string(dev->dev.bda, MAC_BYTES, name);
    } else {
        name = malloc(sizeof(char) * (dev->dev.bdname_len + 1));
        strncpy(name, dev->dev.bdname, dev->dev.bdname_len);
    }
    if (dev->view == NULL) {
        /* Add a new item */
        dev->view = variable_item_list_add(
            app->devices_var_item_list,
            name,
            1,
            wendigo_scene_device_list_var_list_change_callback,
            app);
    } else {
        /* Update dev->view */
        variable_item_set_item_label(dev->view, name);
    }
    // TODO: If this thing crashes it's probably because of this (I can't find the def of furi_string_set to check)
    free(name);
}

/* Initialise the device list
   TODO: When "display options" are implemented include consideration of selected device types and sorting options
*/
void wendigo_scene_device_list_on_enter(void* context) {
    WendigoApp* app = context;
    app->current_view = WendigoAppViewDeviceList;
    VariableItemList* var_item_list = app->devices_var_item_list;

    variable_item_list_set_enter_callback(
        var_item_list, wendigo_scene_device_list_var_list_enter_callback, app);

    variable_item_list_reset(var_item_list);
    char *item_str;
    uint16_t device_count;
    flipper_bt_device **devices;
    if (display_selected_only) {
        device_count = bt_selected_devices_count;
        devices = bt_selected_devices;
    } else {
        device_count = bt_devices_count;
        devices = bt_devices;
    }
    for(int i = 0; i < device_count; ++i) {
        /* Label with the name if we have a name, otherwise use the BDA */
        if (devices[i]->dev.bdname_len > 0 && devices[i]->dev.bdname != NULL) {
            item_str = devices[i]->dev.bdname;
        } else {
            item_str = malloc(sizeof(char) * (MAC_STRLEN + 1));
            if (item_str == NULL) {
                // TODO: Panic
                continue; // Maybe the next device will have a name so we won't need to malloc
            }
            bytes_to_string(devices[i]->dev.bda, MAC_BYTES, item_str);
        }
        devices[i]->view = variable_item_list_add(
            var_item_list,
            item_str,
            1,
            wendigo_scene_device_list_var_list_change_callback,
            app);
        if (devices[i]->dev.bdname_len == 0 || devices[i]->dev.bdname == NULL) {
            free(item_str);
        }
    }
    // TODO: Display WiFi devices

    variable_item_list_set_selected_item(
        var_item_list, scene_manager_get_scene_state(app->scene_manager, WendigoSceneDeviceList));

    view_dispatcher_switch_to_view(app->view_dispatcher, WendigoAppViewDeviceList);
}

bool wendigo_scene_device_list_on_event(void* context, SceneManagerEvent event) {
    WendigoApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch (event.event) {
            case Wendigo_EventListDevices:
                scene_manager_set_scene_state(
                    app->scene_manager, WendigoSceneDeviceList, app->device_list_selected_menu_index);
                scene_manager_next_scene(app->scene_manager, WendigoSceneDeviceDetail);
                break;
            default:
                // TODO: Panic
                break;
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
    for (int i = 0; i < bt_devices_count; ++i) {
        bt_devices[i]->view = NULL;
    }
}
