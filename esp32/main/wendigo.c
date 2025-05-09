/* Basic console example (esp_console_repl API)

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "wendigo.h"
#include "common.h"
#include "wifi.h"
#include "bluetooth.h"

/*
 * We warn if a secondary serial console is enabled. A secondary serial console is always output-only and
 * hence not very useful for interactive console applications. If you encounter this warning, consider disabling
 * the secondary serial console in menuconfig unless you know what you are doing.
 */
#if SOC_USB_SERIAL_JTAG_SUPPORTED
#if !CONFIG_ESP_CONSOLE_SECONDARY_NONE
#warning "A secondary serial console is not useful when using the console component. Please disable it in menuconfig."
#endif
#endif

#define PROMPT_STR CONFIG_IDF_TARGET

/* Console command history can be stored to and loaded from a file.
 * The easiest way to do this is to use FATFS filesystem on top of
 * wear_levelling library.
 */
#if CONFIG_CONSOLE_STORE_HISTORY
    #define MOUNT_PATH "/data"
    #define HISTORY_PATH MOUNT_PATH "/history.txt"
#endif // CONFIG_STORE_HISTORY

/* Display command syntax when in interactive mode */
void display_syntax(char *command) {
    printf("Usage: %s ( %d | %d | %d )\n%d: %s\n%d: %s\n%d: %s\n", command, ACTION_DISABLE,
           ACTION_ENABLE, ACTION_STATUS, ACTION_DISABLE, STRING_DISABLE, ACTION_ENABLE,
           STRING_ENABLE, ACTION_STATUS, STRING_STATUS);
}

/* Inform the caller of an invalid command
   Either by displaying the command syntax or sending a MSG_INVALID response
*/
void invalid_command(char *cmd, char *arg, char *syntax) {
    if (scanStatus[SCAN_INTERACTIVE] == ACTION_ENABLE) {
        display_syntax(syntax);
    } else {
        send_response(cmd, arg, MSG_INVALID);
    }
}

/* Sends a `result` message corresponding to the command `cmd` with optional argument `arg` over UART
   unless interactive mode is enabled, in which case a more readable result is printed to the console. */
esp_err_t send_response(char *cmd, char *arg, MsgType result) {
    char resultMsg[17];
    bool isError = false;
    switch (result) {
        case MSG_ACK:
            isError = false;
            if (scanStatus[SCAN_INTERACTIVE] == ACTION_ENABLE) {
                strcpy(resultMsg, "Executing:");
            } else {
                strcpy(resultMsg, "ACK");
            }
            break;
        case MSG_OK:
            isError = false;
            if (scanStatus[SCAN_INTERACTIVE] == ACTION_ENABLE) {
                strcpy(resultMsg, "Success:");
            } else {
                strcpy(resultMsg, "OK");
            }
            break;
        case MSG_FAIL:
            isError = true;
            if (scanStatus[SCAN_INTERACTIVE] == ACTION_ENABLE) {
                strcpy(resultMsg, "Error Running");
            } else {
                strcpy(resultMsg, "FAIL");
            }
            break;
        case MSG_INVALID:
            isError = true;
            if (scanStatus[SCAN_INTERACTIVE] == ACTION_ENABLE) {
                strcpy(resultMsg, "Invalid Command:");
            } else {
                strcpy(resultMsg, "INVALID");
            }
            break;
        default:
            ESP_LOGE(TAG, "Invalid result type: %d\n", result);
            return ESP_ERR_INVALID_ARG;
    }
    /* To display the message we want to include 2 strings if arg is NULL, otherwise 3, and to order
       printf arguments resultMsg, cmd, arg for Interactive mode; cmd, arg, resultMsg for UART mode. */
    if (arg == NULL) {
        if (scanStatus[SCAN_INTERACTIVE] == ACTION_ENABLE) {
            if (isError) {
                ESP_LOGE(TAG, "%s %s\n", resultMsg, cmd);
            } else {
                /* Not currently displaying non-error messages to interactive console */
                //ESP_LOGI(TAG, "%s %s\n", resultMsg, cmd);
            }
        } else {
            printf("%s %s\n", cmd, resultMsg);
        }
    } else {
        if (scanStatus[SCAN_INTERACTIVE] == ACTION_ENABLE) {
            if (isError) {
                ESP_LOGE(TAG, "%s %s %s\n", resultMsg, cmd, arg);
            } else {
                /* Not currently displaying non-error messages to interactive console */
                //ESP_LOGI(TAG, "%s %s %s\n", resultMsg, cmd, arg);
            }
        } else {
            printf("%s %s %s\n", cmd, arg, resultMsg);
        }
    }
    return ESP_OK;
}

/* Command syntax is <command> <ActionType>, where ActionType :== 0 | 1 | 2 */
ActionType parseCommand(int argc, char **argv) {
    /* Validate the argument - Must be between 0 & 2 */
    if (argc != 2) {
        return ACTION_INVALID;
    }
    int action = strtol(argv[1], NULL, 10);
    if (strlen(argv[1]) == 1 && action >= ACTION_DISABLE && action < ACTION_INVALID) {
        return (ActionType)action;
    } else {
        return ACTION_INVALID;
    }
}

/* A generic function that is called by the command handlers for the HCI, BLE, WIFI and INTERACTIVE commands.
   Parses the command line to determine and perform the requested action, sending appropriate messages to the UART
   host throughout. If a command requires `radio` to be enabled or disabled the function pointer `enableFunction()`
   or `disableFunction` is executed. These functions must take no arguments and return esp_err_t (or int).
*/
esp_err_t enableDisableRadios(int argc, char **argv, ScanType radio, esp_err_t (*enableFunction)(), esp_err_t (*disableFunction)()) {
    esp_err_t result = ESP_OK;
    ActionType action = parseCommand(argc, argv);
    if (action == ACTION_INVALID) {
        invalid_command(argv[0], argv[1], syntaxTip[radio]);
        result = ESP_ERR_INVALID_ARG;
    } else {
        /* Acknowledge the message */
        send_response(argv[0], argv[1], MSG_ACK);
        /* Is the radio being turned on, turned off, or queried? */
        switch (action) {
            case ACTION_DISABLE:
                if (scanStatus[radio] == ACTION_ENABLE) {
                    scanStatus[radio] = ACTION_DISABLE;
                    if (disableFunction != NULL) {
                        result = disableFunction();
                    }
                }
                break;
            case ACTION_ENABLE:
                if (scanStatus[radio] == ACTION_DISABLE) {
                    scanStatus[radio] = ACTION_ENABLE;
                    if (enableFunction != NULL) {
                        result = enableFunction();
                    }
                }
                break;
            case ACTION_STATUS:
                if (scanStatus[SCAN_INTERACTIVE] == ACTION_ENABLE) {
                    ESP_LOGI(TAG, "%s Scanning: %s\n", radioFullNames[radio], (scanStatus[radio] == ACTION_ENABLE)?STRING_ACTIVE:STRING_IDLE);
                } else {
                    printf("%s status %d\n", radioShortNames[radio], scanStatus[radio]);
                }
                break;
            default:
                invalid_command(argv[0], argv[1], syntaxTip[radio]);
                result = ESP_ERR_INVALID_ARG;
                break;
        }
    }
    /* Reply with MSG_OK or MSG_FAIL */
    // TODO: Incorporate interactive mode
    MsgType reply = MSG_OK;
    if (result != ESP_OK) {
        reply = MSG_FAIL;
    }
    send_response(argv[0], argv[1], reply);
    return ESP_OK;
}

esp_err_t cmd_bluetooth(int argc, char **argv) {
    enableDisableRadios(argc, argv, SCAN_HCI, wendigo_bt_enable, wendigo_bt_disable);
    return ESP_OK;
}

esp_err_t cmd_ble(int argc, char **argv) {
    enableDisableRadios(argc, argv, SCAN_BLE, wendigo_ble_enable, wendigo_ble_disable);
    return ESP_OK;
}

esp_err_t cmd_wifi(int argc, char **argv) {
    enableDisableRadios(argc, argv, SCAN_WIFI, wendigo_wifi_enable, wendigo_wifi_disable);
    return ESP_OK;
}

/* The `status` command is intended to provide an overall picture of ESP32-Wendigo's current state:
   * Scanning status for each radio
   * Compiled with Bluetooth UUID dictionary (CONFIG_DECODE_UUIDS)
   * ???
   This is only relevant to interactive use.
*/
esp_err_t cmd_status(int argc, char **argv) {
    if (scanStatus[SCAN_INTERACTIVE] == ACTION_ENABLE) {
        #if defined(CONFIG_DECODE_UUIDS)
            char *uuidDictionary = STRING_YES;
        #else
            char *uuidDictionary = STRING_NO;
        #endif
        #if defined(CONFIG_BT_CLASSIC_ENABLED)
            char *btClassicSupport = STRING_YES;
        #else
            char *btClassicSupport = STRING_NO;
        #endif
        #if defined(CONFIG_BT_BLE_ENABLED)
            char *btBLESupport = STRING_YES;
        #else
            char *btBLESupport = STRING_NO;
        #endif
        #if defined(CONFIG_ESP_WIFI_ENABLED) || defined(CONFIG_ESP_HOST_WIFI_ENABLED)
            char *wifiSupport = STRING_YES;
        #else
            char *wifiSupport = STRING_NO;
        #endif
        // TODO: Construct this programmatically. Don't waste so much memory.
        printf("\n*****************************************************\n*              Wendigo \
version %7s              *\n*                Chris Bennetts-Cash                *\n*            \
                                       *\n*                                                   *\n\
*    Compiled With Bluetooth Dictionary: %7s    *\n*    Bluetooth Classic Supported: %14s    *\n\
*    Bluetooth Low Energy Supported: %11s    *\n*    WiFi Supported: %27s    *\n*    Bluetooth C\
lassic Scanning: %15s    *\n*    Bluetooth Low Energy Scanning: %12s    *\n*    WiFi Scanning: \
%28s    *\n*                                                   *\n******************************\
***********************\n",
                WENDIGO_VERSION, uuidDictionary, btClassicSupport, btBLESupport, wifiSupport,
                (strcmp(btClassicSupport, STRING_YES))?STRING_NA:(scanStatus[SCAN_HCI] == ACTION_ENABLE)?STRING_ACTIVE:STRING_IDLE,
                (strcmp(btBLESupport, STRING_YES))?STRING_NA:(scanStatus[SCAN_BLE] == ACTION_ENABLE)?STRING_ACTIVE:STRING_IDLE,
                (strcmp(wifiSupport, STRING_YES))?STRING_NA:(scanStatus[SCAN_WIFI] == ACTION_ENABLE)?STRING_ACTIVE:STRING_IDLE);
    } else {
        /* This command is not valid unless running interactively */
        send_response(argv[0], argv[1], MSG_INVALID);
    }
    return ESP_OK;
}

esp_err_t cmd_version(int argc, char **argv) {
    char msg[17];
    snprintf(msg, sizeof(msg), "Wendigo v%s\n", WENDIGO_VERSION);
    send_response(argv[0], NULL, MSG_ACK);
    if (scanStatus[SCAN_INTERACTIVE] == ACTION_ENABLE) {
        ESP_LOGI(TAG, "%s", msg);
    } else {
        printf(msg);
    }
    send_response(argv[0], NULL, MSG_OK);
    return ESP_OK;
}

esp_err_t cmd_interactive(int argc, char **argv) {
    enableDisableRadios(argc, argv, SCAN_INTERACTIVE, NULL, NULL);
    return ESP_OK;
}

static void initialize_filesystem(void)
{
    static wl_handle_t wl_handle;
    const esp_vfs_fat_mount_config_t mount_config = {
            .max_files = 4,
            .format_if_mount_failed = true
    };
    esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl(MOUNT_PATH, "storage", &mount_config, &wl_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(err));
        return;
    }
}

static void initialize_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

static int register_wendigo_commands() {
    esp_err_t err;
    for (int i=0; i < CMD_COUNT; ++i) {
        err = esp_console_cmd_register(&commands[i]);
        switch (err) {
        case ESP_OK:
            ESP_LOGI(TAG, "Registered command \"%s\"...", commands[i].command);
            break;
        case ESP_ERR_NO_MEM:
            ESP_LOGE(TAG, "Out of memory registering command \"%s\"!", commands[i].command);
            return ESP_ERR_NO_MEM;
        case ESP_ERR_INVALID_ARG:
            ESP_LOGW(TAG, "Invalid arguments provided during registration of \"%s\". Skipping...", commands[i].command);
            continue;
        }
    }
    return ESP_OK;
}

void app_main(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    /* Prompt to be printed before each line.
     * This can be customized, made dynamic, etc.
     */
    repl_config.prompt = PROMPT_STR ">";
    repl_config.max_cmdline_length = CONFIG_CONSOLE_MAX_COMMAND_LINE_LENGTH;

    initialize_nvs();

    #if CONFIG_CONSOLE_STORE_HISTORY
        initialize_filesystem();
        repl_config.history_save_path = HISTORY_PATH;
        ESP_LOGI(TAG, "Command history enabled");
    #else
        ESP_LOGI(TAG, "Command history disabled");
    #endif

    /* Register commands */
    esp_console_register_help_command();
    register_system_common();
    #if SOC_LIGHT_SLEEP_SUPPORTED
        register_system_light_sleep();
    #endif
    #if SOC_DEEP_SLEEP_SUPPORTED
        register_system_deep_sleep();
    #endif
    #if (CONFIG_ESP_WIFI_ENABLED || CONFIG_ESP_HOST_WIFI_ENABLED)
        register_wifi();
    #endif
    register_nvs();

    register_wendigo_commands();

    #if defined(CONFIG_ESP_CONSOLE_UART_DEFAULT) || defined(CONFIG_ESP_CONSOLE_UART_CUSTOM)
        esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));

    #elif defined(CONFIG_ESP_CONSOLE_USB_CDC)
        esp_console_dev_usb_cdc_config_t hw_config = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_console_new_repl_usb_cdc(&hw_config, &repl_config, &repl));

    #elif defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
        esp_console_dev_usb_serial_jtag_config_t hw_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &repl));

    #else
        #error Unsupported console type
    #endif

    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
