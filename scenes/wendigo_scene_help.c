#include "../wendigo_app_i.h"

void wendigo_scene_help_widget_callback(GuiButtonType result, InputType type, void* context) {
    WendigoApp* app = context;
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(app->view_dispatcher, result);
    }
}

void wendigo_scene_help_on_enter(void* context) {
    WendigoApp* app = context;

    FuriString* temp_str;
    temp_str = furi_string_alloc();
    furi_string_printf(
        temp_str,
        "\nWendigo for Flipper\n\nI'm in github: cool4uma\n\nThis app is a modified\nWiFi Marauder companion,\nThanks 0xchocolate(github)\nfor great code and app.\n\n");
    furi_string_cat_printf(temp_str, "Press BACK to return\n");

    widget_add_text_box_element(
        app->widget,
        0,
        0,
        128,
        14,
        AlignCenter,
        AlignBottom,
        "\e#\e!                                                      \e!\n",
        false);
    widget_add_text_box_element(
        app->widget,
        0,
        2,
        128,
        14,
        AlignCenter,
        AlignBottom,
        "\e#\e!         WENDIGO            \e!\n",
        false);
    widget_add_text_scroll_element(app->widget, 0, 16, 128, 50, furi_string_get_cstr(temp_str));
    furi_string_free(temp_str);

    view_dispatcher_switch_to_view(app->view_dispatcher, WendigoAppViewHelp);
}

bool wendigo_scene_help_on_event(void* context, SceneManagerEvent event) {
    WendigoApp* app = context;
    bool consumed = false;
    UNUSED(app);
    UNUSED(event);
    return consumed;
}

void wendigo_scene_help_on_exit(void* context) {
    WendigoApp* app = context;
    // Clear views
    widget_reset(app->widget);
}
