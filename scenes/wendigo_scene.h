#pragma once

#include <gui/scene_manager.h>

// Generate scene id and total number
#define ADD_SCENE(prefix, name, id) WendigoScene##id,
typedef enum {
#include "wendigo_scene_config.h"
    WendigoSceneNum,
} WendigoScene;
#undef ADD_SCENE

extern const SceneManagerHandlers wendigo_scene_handlers;

// Generate scene on_enter handlers declaration
#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void*);
#include "wendigo_scene_config.h"
#undef ADD_SCENE

// Generate scene on_event handlers declaration
#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "wendigo_scene_config.h"
#undef ADD_SCENE

// Generate scene on_exit handlers declaration
#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void* context);
#include "wendigo_scene_config.h"
#undef ADD_SCENE

#define UART_PINS_ITEM_IDX (0)
#define HEX_MODE_ITEM_IDX  (2)

#define DEFAULT_BAUDRATE_OPT_IDX (18)
