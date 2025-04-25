#ifndef WENDIGO_H
#define WENDIGO_H

/* ********************** ESP32 Wendigo ***********************
 * The spiritual successor to Gravity which, trying to be all *
 * things to all wireless protocols, inevitably became a      *
 * juggle between doing things well and doing a lot of        *
 * things.                                                    *
 * WiFi will undoubtedly make an appearance in Wendigo        *
 * because it's fairly straightforward to implement, but the  *
 * initial focus is Bluetooth Classic and Bluetooth Low       *
 * Energy reconnaissance - identifying and monitoring devices *
 * of interest.                                               *
 * The notion of using passive radio signals to track an      *
 * individual interests me because it brilliantly highlights  *
 * the amount of privacy the general public has given up for  *
 * the convenience of constant connectivity, and I don't      *
 * believe there is a general awareness of this.              *
 * Who knows, maybe this will crash & burn, and put my mind   *
 * at rest. *Hopefully* it will, but I doubt it.              *
 *                                                            *
 * ESP32 Wendigo is a lightweight application that provides   *
 * wireless data streams to the Flipper Zero application of   *
 * the same name. This firmware should be used in conjunction *
 * with the Flipper Zero application Wendigo                  *
 * https://github.com/chris-bc/wendigo                        *
 *                                                            *
 * A Wendigo is a fearsome beast that stalks and eats humans, *
 * or a spirit that possesses humans and turns them into      *
 * cannibals, in Native American Algonquian folklore.         *
 *                                                            *
 *                                                            *
 * ESP32 Wendigo: https://github.com/chris-bc/esp32-wendigo   *
 * Licensed under the MIT Open Source License.                *
 *************************************************************/
#define WENDIGO_VERSION "0.1.0"
#define MAX_16_BIT 65535
#define MAX_SSID_LEN 32
#define MAC_STRLEN 17

#include <cmd_nvs.h>
#include <cmd_system.h>
#include <cmd_wifi.h>
#include <esp_console.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_vfs_dev.h>
#include <esp_vfs_fat.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>
#include <freertos/portmacro.h>
#include <nvs.h>
#include <nvs_flash.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

/* Command specifications */
esp_err_t cmd_bluetooth(int argc, char **argv);
esp_err_t cmd_ble(int argc, char **argv);
esp_err_t cmd_wifi(int argc, char **argv);
esp_err_t cmd_status(int argc, char **argv);
esp_err_t cmd_version(int argc, char **argv);

void initPromiscuous();
int initialise_wifi();

bool WIFI_INITIALISED = false;

#define CMD_COUNT 10
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
    }
};

#endif
