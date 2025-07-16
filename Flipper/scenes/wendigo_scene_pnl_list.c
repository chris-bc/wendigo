#include "../wendigo_app_i.h"

/** Scene initialisation.
 */
void wendigo_scene_pnl_list_on_enter(void *context) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_pnl_list_on_enter()");
    WendigoApp *app = context;
    VariableItemList *var_item_list = app->var_item_list;
    app->current_view = WendigoAppViewPNLList;

    variable_item_list_reset(var_item_list);
    VariableItem *item = variable_item_list_add(var_item_list, "Loading...",
                                                1, NULL, app);
    variable_item_set_current_value_index(item, 0);
    variable_item_set_current_value_text(item, "");

    view_dispatcher_switch_to_view(app->view_dispatcher,
                                    WendigoAppViewVarItemList);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_pnl_list_on_enter()");
}

/** We have no need to respond to events */
bool wendigo_scene_pnl_list_on_event(void *context, SceneManagerEvent event) {
    FURI_LOG_T(WENDIGO_TAG, "Start+End wendigo_scene_pnl_list_on_event()");
    UNUSED(context);
    UNUSED(event);
    return false;
}

void wendigo_scene_pnl_list_on_exit(void *context) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_pnl_list_on_exit()");
    WendigoApp *app = context;
    variable_item_list_reset(app->var_item_list);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_pnl_list_on_exit()");
}
