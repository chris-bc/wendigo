#pragma once

#include "wendigo_app.h"
#include "scenes/wendigo_scene.h"
#include "wendigo_custom_event.h"
#include "wendigo_uart.h"

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/text_box.h>
#include <gui/modules/widget.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/text_input.h>
#include <gui/modules/byte_input.h>
#include <gui/modules/popup.h>
#include "wendigo_hex_input.h"

#define START_MENU_ITEMS         (6)
#define SETUP_MENU_ITEMS         (4)
#define SETUP_CHANNEL_MENU_ITEMS (13)

#define MAX_OPTIONS (3)

#define WENDIGO_TEXT_BOX_STORE_SIZE   (4096)
#define WENDIGO_TEXT_INPUT_STORE_SIZE (512)

#define NUM_MAC_BYTES (6)

// Command action type
typedef enum {
    NO_ACTION = 0,
    OPEN_SETUP,
    OPEN_SCAN,
    LIST_DEVICES,
    TRACK_DEVICES,
    OPEN_MAC,
    OPEN_HELP
} ActionType;
// Command availability in different modes
typedef enum {
    OFF = 0,
    TEXT_MODE = 1,
    HEX_MODE = 2,
    BOTH_MODES = 3
} ModeMask;

/* Bitmask for channel selection - Initialised in wendigo_app.c */
static const uint16_t CH_MASK[SETUP_CHANNEL_MENU_ITEMS + 1];
/* Interface types - One for each interface with a MAC */
typedef enum {
    IF_WIFI,
    IF_BLUETOOTH,
} InterfaceType;

typedef struct {
    const char *item_string;
    const char *options_menu[MAX_OPTIONS];
    int num_options_menu;
    ActionType action;
    ModeMask mode_mask;
} WendigoItem;

struct WendigoApp {
    Gui *gui;
    ViewDispatcher *view_dispatcher;
    SceneManager *scene_manager;

    char text_input_store[WENDIGO_TEXT_INPUT_STORE_SIZE + 1];
    FuriString *text_box_store;
    size_t text_box_store_strlen;
    TextBox *text_box;
    TextInput *text_input;
    Wendigo_TextInput *hex_input;
    Widget *widget;
    VariableItemList *var_item_list;
    Wendigo_Uart *uart;
    ByteInput *setup_mac;
    Popup *popup; // Avoid continual allocation and freeing of Popup by initialising at launch
    uint8_t mac_bytes[NUM_MAC_BYTES];
    InterfaceType mac_interface; // What interface is mac_bytes representing?

    int setup_selected_menu_index;
    int setup_selected_option_index[SETUP_MENU_ITEMS];
    int selected_menu_index;
    int selected_option_index[START_MENU_ITEMS];
    const char *selected_tx_string;

    bool is_command;
    bool is_custom_tx_string;
    bool hex_mode;
    uint8_t uart_ch;
    uint8_t new_uart_ch;
    int BAUDRATE;
    int NEW_BAUDRATE;
    int TERMINAL_MODE; //1=AT mode, 0=other mode

    uint16_t channel_mask;
};

typedef enum {
    WendigoAppViewVarItemList,
    WendigoAppViewConsoleOutput,
    WendigoAppViewTextInput,
    WendigoAppViewHexInput,
    WendigoAppViewHelp,
    WendigoAppViewSetupMAC,
    WendigoAppViewSetupChannel,
    WendigoAppViewPopup,
} WendigoAppView;
