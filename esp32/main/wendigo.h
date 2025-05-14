#ifndef WENDIGO_H
#define WENDIGO_H

#include <esp_system.h>
#include <esp_console.h>
#include <esp_vfs_dev.h>
#include <esp_vfs_fat.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <cmd_system.h>
#include <cmd_wifi.h>
#include <cmd_nvs.h>

#include "common.h"

#define WENDIGO_VERSION "0.1.0"

/* Command handlers */
esp_err_t cmd_bluetooth(int argc, char **argv);
esp_err_t cmd_ble(int argc, char **argv);
esp_err_t cmd_wifi(int argc, char **argv);
esp_err_t cmd_status(int argc, char **argv);
esp_err_t cmd_version(int argc, char **argv);
esp_err_t cmd_interactive(int argc, char **argv);
esp_err_t cmd_tag(int argc, char **argv);
esp_err_t cmd_focus(int argc, char **argv);

void invalid_command(char *cmd, char *arg, char *syntax);
void display_syntax(char *command);
esp_err_t send_response(char *cmd, char *arg, MsgType result);
ActionType parseCommand(int argc, char **argv);

#define CMD_COUNT 16
esp_console_cmd_t commands[CMD_COUNT] = {
    {
        .command = "h",
        .hint = "Bluetooth Classic Commands",
        .help = "The `h(ci)` command allows management of the Host Controller Interface - Bluetooth Classic",
        .func = cmd_bluetooth
    }, {
        .command = "hci",
        .hint = "Bluetooth Classic Commands",
        .help = "The `hci` command allows management of the Host Controller Interface - Bluetooth Classic",
        .func = cmd_bluetooth
    }, {
        .command = "b",
        .hint = "BLE Commands",
        .help = "The `b(le)` command allows management of the Bluetooth Low Energy interface (BLE)",
        .func = cmd_ble
    }, {
        .command = "ble",
        .hint = "BLE Commands",
        .help = "The `ble` command allows management of the Bluetooth Low Energy interface (BLE)",
        .func = cmd_ble
    }, {
        .command = "w",
        .hint = "WiFi Commands",
        .help = "The `w(ifi)` command allows management of the WiFi interface",
        .func = cmd_wifi
    }, {
        .command = "wifi",
        .hint = "WiFi Commands",
        .help = "The `wifi` command allows management of the WiFi interface",
        .func = cmd_wifi
    }, {
        .command = "t",
        .hint = "Tagging Commands",
        .help = "The `t[ag]` command allows you to tag and untag devices.",
        .func = cmd_tag
    }, {
        .command = "tag",
        .hint = "Tagging Commands",
        .help = "The `tag` command allows you to tag and untag devices.",
        .func = cmd_tag
    }, {
        .command = "f",
        .hint = "Toggle f[ocus] Mode",
        .help = "Focus Mode limits the display to tagged devices only, and provides output focused on RSSI, allowing devices to be located using their radio signal.",
        .func = cmd_focus
    }, {
        .command = "focus",
        .hint = "Toggle Focus Mode",
        .help = "Focus Mode limits the display to tagged devices only, and provides output focused on RSSI, allowing devices to be located using their radio signal.",
        .func = cmd_focus
    }, {
        .command = "s",
        .hint = "Status Information",
        .help = "The `s(tatus)` command can be used to retrieve and modify certain status and configuration details",
        .func = cmd_status
    }, {
        .command = "status",
        .hint = "Status Information",
        .help = "The `status` command can be used to retrieve and modify certain status and configuration details",
        .func = cmd_status
    }, {
        .command = "v",
        .hint = "Wendigo Version",
        .help = "The `v(ersion)` command displays esp32-Wendigo version information",
        .func = cmd_version
    }, {
        .command = "ver",
        .hint = "Wendigo Version",
        .help = "The `ver(sion)` command displays esp32-Wendigo version information",
        .func = cmd_version
    }, {
        .command = "i",
        .hint = "Toggle Interactive Mode",
        .help = "Toggle i[nteractive] mode",
        .func = cmd_interactive
    }, {
        .command = "interactive",
        .hint = "Toggle Interactive Mode",
        .help = "Toggle i[nteractive] mode",
        .func = cmd_interactive
    }
};

#endif
