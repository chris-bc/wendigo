#include "../wendigo_app_i.h"
#include "../wendigo_scan.h"

static void wendigo_scene_status_var_list_enter_callback(void* context, uint32_t index) {
    furi_assert(context);
    WendigoApp* app = context;
    UNUSED(app);
    UNUSED(index);

}

static void wendigo_scene_status_var_list_change_callback(VariableItem* item) {
    furi_assert(item);

    WendigoApp* app = variable_item_get_context(item);
    furi_assert(app);

}

/** Perform tasks required after all status attributes have been added to var_item_list.
 * This is called following the complete parsing of a status packet to perform any tasks
 * that need to wait until the full set of attributes is displayed, such as setting the
 * currently-selected item based on the last time the scene was viewed.
 */
void wendigo_scene_status_finish_layout(WendigoApp *app) {
    /* Set the selected menu item to what it was last time we were here */
    variable_item_list_set_selected_item(
        app->var_item_list, scene_manager_get_scene_state(app->scene_manager, WendigoSceneStatus));
}

/** Add a variable item for `name` with selected option `value` */
void wendigo_scene_status_add_attribute(WendigoApp *app, char *name, char *value) {
    VariableItem *item = variable_item_list_add(app->var_item_list, name, 1,
        wendigo_scene_status_var_list_change_callback, app);
    variable_item_set_current_value_index(item, 0);
    variable_item_set_current_value_text(item, value);
}

void wendigo_scene_status_begin_layout(WendigoApp *app) {
    variable_item_list_reset(app->var_item_list);
}

void wendigo_scene_status_on_enter(void* context) {
    WendigoApp* app = context;
    VariableItemList* var_item_list = app->var_item_list;
    app->current_view = WendigoAppViewStatus;

    variable_item_list_set_enter_callback(
        var_item_list, wendigo_scene_status_var_list_enter_callback, app);

    variable_item_list_reset(var_item_list);
    VariableItem* item = variable_item_list_add(var_item_list, "Loading...", 1,
        wendigo_scene_status_var_list_change_callback, app);
    variable_item_set_current_value_index(item, 0);
    variable_item_set_current_value_text(item, "");

    // variable_item_list_set_selected_item(
    //     var_item_list, scene_manager_get_scene_state(app->scene_manager, WendigoSceneStatus));

    /* Send the UART command */
    wendigo_uart_set_binary_cb(app->uart);
    wendigo_esp_status(app);

    view_dispatcher_switch_to_view(app->view_dispatcher, WendigoAppViewVarItemList);
}

bool wendigo_scene_status_on_event(void* context, SceneManagerEvent event) {
    WendigoApp* app = context;
    bool consumed = false;

    // TODO: Determine whether I'll have any sub-views
    // if(event.type == SceneManagerEventTypeCustom) {
    //     if(event.event == Wendigo_EventSetup) {
    //         /* Save scene state */
    //         scene_manager_set_scene_state(
    //             app->scene_manager, WendigoSceneSetup, app->setup_selected_menu_index);
    //         scene_manager_next_scene(app->scene_manager, WendigoSceneSetupChannel);
    //     } else if(event.event == Wendigo_EventMAC) {
    //         scene_manager_set_scene_state(
    //             app->scene_manager, WendigoSceneSetup, app->setup_selected_menu_index);
    //         scene_manager_next_scene(app->scene_manager, WendigoSceneSetupMAC);
    //     }
    //     consumed = true;
    // } else if(event.type == SceneManagerEventTypeTick) {
    //     app->setup_selected_menu_index =
    //         variable_item_list_get_selected_item_index(app->var_item_list);
    //     consumed = true;
    // }

    if (event.type == SceneManagerEventTypeTick) {
        app->selected_menu_index = variable_item_list_get_selected_item_index(app->var_item_list);
        consumed = true;
    }

    return consumed;
}

void wendigo_scene_status_on_exit(void* context) {
    WendigoApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
