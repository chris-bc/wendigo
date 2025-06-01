#include "../wendigo_app_i.h"

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

void wendigo_scene_status_on_enter(void* context) {
    WendigoApp* app = context;
    VariableItemList* var_item_list = app->var_item_list;
    app->current_view = WendigoAppViewStatus;

    variable_item_list_set_enter_callback(
        var_item_list, wendigo_scene_status_var_list_enter_callback, app);

    variable_item_list_reset(var_item_list);
    VariableItem* item = variable_item_list_add(var_item_list, "Foo", 1, wendigo_scene_status_var_list_change_callback, app);
    UNUSED(item);
    // for(int i = 0; i < SETUP_MENU_ITEMS; ++i) {
    //     item = variable_item_list_add(
    //         var_item_list,
    //         items[i].item_string,
    //         items[i].num_options_menu,
    //         wendigo_scene_status_var_list_change_callback,
    //         app);
    //     variable_item_set_current_value_index(item, app->setup_selected_option_index[i]);
    //     variable_item_set_current_value_text(
    //         item, items[i].options_menu[app->setup_selected_option_index[i]]);
    // }

    variable_item_list_set_selected_item(
        var_item_list, scene_manager_get_scene_state(app->scene_manager, WendigoSceneStatus));

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
