#include "../wendigo_app_i.h"

wendigo_device *current_device = NULL;

void wendigo_scene_pnl_list_set_device(wendigo_device *dev) {
    current_device = dev;
    // TODO: if (app->current_view == WendigoAppViewPNLList) redraw() - Need to get context (WendigoApp *)
}

wendigo_device *wendigo_scene_pnl_list_get_device() {
    return current_device;
}

void wendigo_scene_pnl_list_redraw(WendigoApp *app) {
    VariableItemList *var_item_list = app->var_item_list;
    variable_item_list_reset(var_item_list);

    if (current_device == NULL || current_device->scanType != SCAN_WIFI_STA) {
        return;
    }
    VariableItem *item;
    if (current_device->radio.sta.saved_networks_count == 0 ||
            current_device->radio.sta.saved_networks == NULL) {
        item = variable_item_list_add(var_item_list, "No networks found", 1,
            NULL, app);
        variable_item_set_current_value_index(item, 0);
        variable_item_set_current_value_text(item, "");
        return;
    }
    // TODO
    for (uint8_t i = 0; i < current_device->radio.sta.saved_networks_count; ++i) {
        if (current_device->radio.sta.saved_networks[i] != NULL) {
            item = variable_item_list_add(var_item_list,
                current_device->radio.sta.saved_networks[i], 1, NULL, app);
            variable_item_set_current_value_index(item, 0);
            variable_item_set_current_value_text(item, "");
        }
    }
}

/** Scene initialisation.
 */
void wendigo_scene_pnl_list_on_enter(void *context) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_pnl_list_on_enter()");
    WendigoApp *app = context;
    app->current_view = WendigoAppViewPNLList;

    wendigo_scene_pnl_list_redraw(app);

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
