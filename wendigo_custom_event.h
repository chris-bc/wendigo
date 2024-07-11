#pragma once

typedef enum {
    Wendigo_EventRefreshConsoleOutput = 0,
    Wendigo_EventSetup,
    Wendigo_EventMAC,
    Wendigo_EventStartConsole,
    Wendigo_EventStartKeyboardText,
    Wendigo_EventStartKeyboardHex,
    Wendigo_EventStartHelp,
} Wendigo_CustomEvent;
