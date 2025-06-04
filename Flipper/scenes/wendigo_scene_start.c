#include "../wendigo_app_i.h"
#include "../wendigo_scan.h"
#include <dolphin/dolphin.h>

// NUM_MENU_ITEMS defined in wendigo_app_i.h - if you add an entry here, increment it!
static const WendigoItem items[START_MENU_ITEMS] = {
    {"Setup", {""}, 1, OPEN_SETUP, BOTH_MODES},
    {"Scan", {"Start", "Stop", "Status"}, 3, OPEN_SCAN, TEXT_MODE},
    {"Devices", {""}, 1, LIST_DEVICES, BOTH_MODES},
    {"Selected Devices", {""}, 1, LIST_SELECTED_DEVICES, BOTH_MODES},
    {"Track Selected", {""}, 1, TRACK_DEVICES, TEXT_MODE},
    {"Help", {"About", "ESP Version"}, 2, OPEN_HELP, TEXT_MODE},
};

#define SETUP_IDX       (0)
#define SCAN_IDX        (1)
#define SCAN_START_IDX  (0)
#define SCAN_STOP_IDX   (1)
#define SCAN_STATUS_IDX (2)
#define ABOUT_IDX       (0)
#define ESP_VER_IDX     (1)
#define LOCKED_MSG      "Stop\nScanning\nFirst!"

static uint8_t menu_items_num = 0;
static uint8_t item_indexes[START_MENU_ITEMS] = {0};

/* Determine whether we're displaying all, or only tagged, devices
   Defined and used in wendigo_scene_device_list.c */
extern bool display_selected_only;

/* Callback invoked when the action button is pressed on a menu item */
static void wendigo_scene_start_var_list_enter_callback(void* context, uint32_t index) {
    furi_assert(context);
    WendigoApp* app = context;

    furi_assert(index < menu_items_num);
    uint8_t item_index = item_indexes[index];
    furi_assert(item_index < START_MENU_ITEMS);
    const WendigoItem* item = &items[item_index];

    const int selected_option_index = app->selected_option_index[index];
    furi_assert(selected_option_index < item->num_options_menu);
    // TODO: Remove these (I think)
    app->selected_tx_string = item->options_menu[selected_option_index];
    app->is_command = false;
    app->is_custom_tx_string = false;
    app->selected_menu_index = index;

    /* Reward Dolphin for running a command */
    dolphin_deed(DolphinDeedGpioUartBridge);

    switch(item->action) {
    case OPEN_SETUP:
        view_dispatcher_send_custom_event(app->view_dispatcher, Wendigo_EventSetup);
        return;
    case OPEN_SCAN:
        VariableItem* myItem;
        /* Quit now if there's nothing to do */
        if ((selected_option_index == SCAN_START_IDX && app->is_scanning) ||
            (selected_option_index == SCAN_STOP_IDX && !app->is_scanning)) {
            break;
        }
        bool starting = (selected_option_index == SCAN_START_IDX);
        /* Disable/Enable settings menu when starting/stopping scanning */
        if (selected_option_index == SCAN_START_IDX || selected_option_index == SCAN_STOP_IDX) {
            myItem = variable_item_list_get(app->var_item_list, SETUP_IDX);
            variable_item_set_locked(myItem, starting, LOCKED_MSG);
            /* Set selected option to Stop when starting and Start when stopping */
            myItem = variable_item_list_get(app->var_item_list, SCAN_IDX);
            uint8_t newOption = (starting) ? SCAN_STOP_IDX : SCAN_START_IDX;
            app->selected_option_index[index] = newOption;
            variable_item_set_current_value_index(myItem, newOption);
            variable_item_set_current_value_text(myItem, item->options_menu[newOption]);
            wendigo_set_scanning_active(app, starting);
        } else if (selected_option_index == SCAN_STATUS_IDX) {
            view_dispatcher_send_custom_event(app->view_dispatcher, Wendigo_EventDisplayStatus);
            return;
        }
        break;
    case LIST_DEVICES:
        // Try to fit name, BDA and CoD on the var_list
        // Allow selecting a device to obtain more information: remaining attributes, tag/untag, services, maybe some transmit options
        display_selected_only = false;
        view_dispatcher_send_custom_event(app->view_dispatcher, Wendigo_EventListDevices);
        return;
    case LIST_SELECTED_DEVICES:
        display_selected_only = true;
        view_dispatcher_send_custom_event(app->view_dispatcher, Wendigo_EventListDevices);
        return;
    case TRACK_DEVICES:
        view_dispatcher_send_custom_event(app->view_dispatcher, Wendigo_EventStartConsole);
        return;
    case OPEN_HELP:
        switch (selected_option_index) {
            case ABOUT_IDX:
                view_dispatcher_send_custom_event(app->view_dispatcher, Wendigo_EventStartHelp);
                break;
            case ESP_VER_IDX:
                /* Ensure wendigo_scan.c receives transmitted data */
                // TODO: The UART callback is only modified by wendigo_app.c and wendigo_scene_console_output.c - If ConsoleOutput is removed then this can be
                wendigo_uart_set_binary_cb(app->uart);
                wendigo_esp_version(app);
                break;
            default:
                // TODO: Panic
                break;
        }
        return;
    default:
        return;
    }
}

/* Callback invoked when a menu option is changed */
static void wendigo_scene_start_var_list_change_callback(VariableItem* item) {
    furi_assert(item);

    WendigoApp* app = variable_item_get_context(item);
    furi_assert(app);

    uint8_t item_index = item_indexes[app->selected_menu_index];
    const WendigoItem* menu_item = &items[item_index];
    uint8_t option_index = variable_item_get_current_value_index(item);
    furi_assert(option_index < menu_item->num_options_menu);
    variable_item_set_current_value_text(item, menu_item->options_menu[option_index]);
    app->selected_option_index[app->selected_menu_index] = option_index;
}

/* Callback invoked when the view is launched */
void wendigo_scene_start_on_enter(void* context) {
    WendigoApp* app = context;
    VariableItemList* var_item_list = app->var_item_list;
    app->current_view = WendigoAppViewVarItemList;

    for(int i = 0; i < START_MENU_ITEMS; ++i) {
        app->selected_option_index[i] = 0;
    }

    variable_item_list_set_enter_callback(
        var_item_list, wendigo_scene_start_var_list_enter_callback, app);

    VariableItem* item;
    menu_items_num = 0;
    for(int i = 0; i < START_MENU_ITEMS; ++i) {
        bool enabled = false;
        if(app->hex_mode && (items[i].mode_mask & HEX_MODE)) {
            enabled = true;
        }
        if(!app->hex_mode && (items[i].mode_mask & TEXT_MODE)) {
            enabled = true;
        }

        if(enabled) {
            item = variable_item_list_add(
                var_item_list,
                items[i].item_string,
                items[i].num_options_menu,
                wendigo_scene_start_var_list_change_callback,
                app);
            variable_item_set_current_value_index(item, app->selected_option_index[i]);
            variable_item_set_current_value_text(
                item, items[i].options_menu[app->selected_option_index[i]]);

            item_indexes[menu_items_num] = i;
            menu_items_num++;
            if (i == SETUP_IDX && app->is_scanning) {
                variable_item_set_locked(item, true, LOCKED_MSG);
            }
        }
    }

    variable_item_list_set_selected_item(
        var_item_list, scene_manager_get_scene_state(app->scene_manager, WendigoSceneStart));

    view_dispatcher_switch_to_view(app->view_dispatcher, WendigoAppViewVarItemList);
}

bool wendigo_scene_start_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    WendigoApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch (event.event) {
            case Wendigo_EventSetup:
                scene_manager_set_scene_state(
                    app->scene_manager, WendigoSceneStart, app->selected_menu_index);
                scene_manager_next_scene(app->scene_manager, WendigoSceneSetup);
                break;
            case Wendigo_EventStartKeyboardText:
                scene_manager_set_scene_state(
                    app->scene_manager, WendigoSceneStart, app->selected_menu_index);
                scene_manager_next_scene(app->scene_manager, WendigoSceneTextInput);
                break;
            case Wendigo_EventStartKeyboardHex:
                scene_manager_set_scene_state(
                    app->scene_manager, WendigoSceneStart, app->selected_menu_index);
                scene_manager_next_scene(app->scene_manager, WendigoSceneHexInput);
                break;
            case Wendigo_EventStartConsole:
                scene_manager_set_scene_state(
                    app->scene_manager, WendigoSceneStart, app->selected_menu_index);
                scene_manager_next_scene(app->scene_manager, WendigoSceneConsoleOutput);
                break;
            case Wendigo_EventStartHelp:
                scene_manager_set_scene_state(
                    app->scene_manager, WendigoSceneStart, app->selected_menu_index);
                scene_manager_next_scene(app->scene_manager, WendigoSceneHelp);
                break;
            case Wendigo_EventListDevices:
                scene_manager_set_scene_state(
                    app->scene_manager, WendigoSceneStart, app->selected_menu_index);
                scene_manager_next_scene(app->scene_manager, WendigoSceneDeviceList);
                break;
            case Wendigo_EventDisplayStatus:
                scene_manager_set_scene_state(
                    app->scene_manager, WendigoSceneStart, app->selected_menu_index);
                scene_manager_next_scene(app->scene_manager, WendigoSceneStatus);
                break;
            default:
                // Do nothing
                break;
        }
        consumed = true;
    } else if(event.type == SceneManagerEventTypeTick) {
        app->selected_menu_index = variable_item_list_get_selected_item_index(app->var_item_list);
        consumed = true;
    }
    return consumed;
}

void wendigo_scene_start_on_exit(void* context) {
    WendigoApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
