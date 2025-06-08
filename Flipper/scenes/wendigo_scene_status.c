#include "../wendigo_app_i.h"
#include "../wendigo_scan.h"

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
    VariableItem *item = variable_item_list_add(app->var_item_list, name, 1, NULL, app);
    variable_item_set_current_value_index(item, 0);
    variable_item_set_current_value_text(item, value);
}

/** Performs activities to prepare the scene for items to be added to it.
 *  Currently it just purges the existing var_item_list */
void wendigo_scene_status_begin_layout(WendigoApp *app) {
    variable_item_list_reset(app->var_item_list);
}

/** Scene initialisation - Unlike most variable_item_list scenes, this function
 * displays a placeholder menu item and sends a UART message asking ESP32-Wendigo
 * to send us status information.
 */
void wendigo_scene_status_on_enter(void* context) {
    WendigoApp* app = context;
    VariableItemList* var_item_list = app->var_item_list;
    app->current_view = WendigoAppViewStatus;

    variable_item_list_reset(var_item_list);
    VariableItem* item = variable_item_list_add(var_item_list, "Loading...", 1, NULL, app);
    variable_item_set_current_value_index(item, 0);
    variable_item_set_current_value_text(item, "");

    /* Send the UART command */
    wendigo_uart_set_binary_cb(app->uart);
    wendigo_esp_status(app);

    view_dispatcher_switch_to_view(app->view_dispatcher, WendigoAppViewVarItemList);
}

bool wendigo_scene_status_on_event(void* context, SceneManagerEvent event) {
    bool consumed = false;
    UNUSED(context);

    if (event.type == SceneManagerEventTypeTick) {
        /* Nothing for us to do but acknowledge the tick */
        consumed = true;
    }
    return consumed;
}

void wendigo_scene_status_on_exit(void* context) {
    WendigoApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
