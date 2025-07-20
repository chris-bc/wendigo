/* Basic console example (esp_console_repl API)

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "wendigo.h"
#include "common.h"
#include "freertos/idf_additions.h"
#include "wifi.h"
#include "bluetooth.h"
#include "status.h"
#include <driver/uart_vfs.h>
/* Required in order to disable command hints */
#include "linenoise/linenoise.h"

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

/** Display command syntax when in interactive mode */
void display_syntax(char *command) {
    if (scanStatus[SCAN_INTERACTIVE] == ACTION_ENABLE) {
        printf("Usage: %s ( %d | %d | %d )\n%d: %s\n%d: %s\n%d: %s\n", command, ACTION_DISABLE,
               ACTION_ENABLE, ACTION_STATUS, ACTION_DISABLE, STRING_DISABLE, ACTION_ENABLE,
               STRING_ENABLE, ACTION_STATUS, STRING_STATUS);
    }
}

/** Inform the caller of an invalid command
 *  Either by displaying the command syntax or sending a MSG_INVALID response
 */
void invalid_command(char *cmd, char *arg, char *syntax) {
    if (scanStatus[SCAN_INTERACTIVE] == ACTION_ENABLE) {
        display_syntax(syntax);
    } else {
        send_response(cmd, arg, MSG_INVALID);
    }
}

/** Sends a `result` message corresponding to the command `cmd` with optional argument `arg` over UART
 *  unless interactive mode is enabled, in which case a more readable result is printed to the console.
 */
esp_err_t send_response(char *cmd, char *arg, MsgType result) {
    // TODO: Decide whether or not to keep this and refactor as appropriate
    // As I begin implementing the FZ side of Wendigo I'm reconsidering the
    // usefulness of the ACK/OK/INVALID/FAIL protocol. Disable these for now
    if (scanStatus[SCAN_INTERACTIVE] == ACTION_DISABLE) {
        return ESP_OK;
    }

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

/** Command syntax is <command> <ActionType>, where ActionType :== 0 | 1 | 2 */
ActionType parseCommand(int argc, char **argv) {
    /* Validate the argument - Must be between 0 & 2 */
    if (argc != 2) {
        return ACTION_INVALID;
    }
    uint8_t action = strtol(argv[1], NULL, 10);
    if (strlen(argv[1]) == 1 && action < ACTION_INVALID) {
        return (ActionType)action;
    } else {
        return ACTION_INVALID;
    }
}

/** Tag syntax is t[ag] ( b[t] | w[ifi] ) <MAC> <ActionType>, where ActionType :== 0 | 1 | 2 (or 3 for ACTION_INVALID) */
ActionType parse_command_tag(int argc, char **argv, esp_bd_addr_t addr) {
    if (argc != 4) {
        return ACTION_INVALID;
    }
    /* argv[1] must be b, bt, w, or wifi */
    if (strcasecmp(argv[1], "b") && strcasecmp(argv[1], "bt") &&
        strcasecmp(argv[1], "w") && strcasecmp(argv[1], "wifi")) {
        return ACTION_INVALID;
    }
    /* argv[2] must be a MAC/BDA - Check it has the right number of bytes
       (and won't buffer overflow etc).
    */
    if (((strlen(argv[2]) + 1) / 3) != ESP_BD_ADDR_LEN) {
        return ACTION_INVALID;
    }
    esp_err_t result = wendigo_string_to_bytes(argv[2], addr);
    if (result != ESP_OK) {
        return ACTION_INVALID;
    }
    /* argv[3] must contain a valid ActionType enum */
    uint8_t action = strtol(argv[3], NULL, 10);
    if (strlen(argv[3]) == 1 && action < ACTION_INVALID) {
        return (ActionType)action;
    } else {
        return ACTION_INVALID;
    }
}

/** Sets Wendigo's logging output to the specified level.
 *  On launch all logging is disabled, to prevent logs being sent over the
 *  UART interface. This function is used to automatically enable/disable
 *  logging when Interactive Mode is started/stopped.
 */
void wendigo_set_logging(esp_log_level_t level) {
    esp_log_level_set(BLE_TAG, level);
    esp_log_level_set(BT_TAG, level);
    esp_log_level_set(TAG, level);
}

/** A generic function that is called by the command handlers for the HCI, BLE, WIFI and INTERACTIVE commands.
 *  Parses the command line to determine and perform the requested action, sending appropriate messages to the UART
 *  host throughout. If a command requires `radio` to be enabled or disabled the function pointer `enableFunction()`
 *  or `disableFunction` is executed. These functions must take no arguments and return esp_err_t (or int).
 */
esp_err_t enableDisableRadios(int argc, char **argv, uint8_t radio, esp_err_t (*enableFunction)(), esp_err_t (*disableFunction)()) {
    esp_err_t result = ESP_OK;
    if (radio >= SCAN_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
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
    enableDisableRadios(argc, argv, SCAN_WIFI_AP, wendigo_wifi_enable, wendigo_wifi_disable);
    /* This sets scanStatus[] - Don't need to call the enable/disable function again */
    enableDisableRadios(argc, argv, SCAN_WIFI_STA, NULL, NULL);
    return ESP_OK;
}

esp_err_t cmd_channel(int argc, char **argv) {
    if (argc == 1) {
        return wendigo_get_channels();
    } else {
        /* Convert argv[] to a uint8_t[] (excluding argv[0]) */
        uint8_t *channel_list = malloc(argc - 1);
        uint8_t channel_count = 0;
        uint8_t this_channel;
        if (channel_list == NULL) {
            return ESP_ERR_NO_MEM;
        }
        for (uint8_t i = 1; i < argc; ++i) {
            // TODO: Is this cast always safe? Maybe I should check bounds before casting?
            this_channel = (uint8_t)strtol(argv[i], NULL, 10);
            if (wendigo_is_valid_channel(this_channel)) {
                channel_list[channel_count++] = this_channel;
            }
        }
        esp_err_t result = wendigo_set_channels(channel_list, channel_count);
        free(channel_list);
        if (result == ESP_ERR_NO_MEM) {
            outOfMemory();
        }
        return result;
    }
}

/** The `status` command is intended to provide an overall picture of ESP32-Wendigo's current state:
 *  * Support for each radio
 *  * Scanning status for each radio
 *  * Compiled with Bluetooth UUID dictionary (CONFIG_DECODE_UUIDS)
 *  * Metrics of device caches
 */
esp_err_t cmd_status(int argc, char **argv) {
    #if defined(CONFIG_DECODE_UUIDS)
        bool uuidDictionarySupported = true;
    #else
        bool uuidDictionarySupported = false;
    #endif
    #if defined(CONFIG_BT_CLASSIC_ENABLED)
        bool btClassicSupported = true;
    #else
        bool btClassicSupported = false;
    #endif
    #if defined(CONFIG_BT_BLE_ENABLED)
        bool btBLESupported = true;
    #else
        bool btBLESupported = false;
    #endif
    #if defined(CONFIG_ESP_WIFI_ENABLED) || defined(CONFIG_ESP_HOST_WIFI_ENABLED)
        bool wifiSupported = true;
    #else
        bool wifiSupported = false;
    #endif
    if (scanStatus[SCAN_INTERACTIVE] == ACTION_ENABLE) {
        display_status_interactive(uuidDictionarySupported, btClassicSupported, btBLESupported, wifiSupported);
    } else {
        display_status_uart(uuidDictionarySupported, btClassicSupported, btBLESupported, wifiSupported);
    }
    return ESP_OK;
}

esp_err_t cmd_version(int argc, char **argv) {
    char msg[17];
    snprintf(msg, sizeof(msg), "Wendigo v%s", WENDIGO_VERSION);
    send_response(argv[0], NULL, MSG_ACK);
    if (scanStatus[SCAN_INTERACTIVE] == ACTION_ENABLE) {
        ESP_LOGI(TAG, "%s", msg);
    } else {
        /* Wait for the talking stick */
        if (xSemaphoreTake(uartMutex, portMAX_DELAY) == pdTRUE) {
            send_bytes((uint8_t *)msg, strlen(msg) + 1);
            send_end_of_packet();
            xSemaphoreGive(uartMutex);
        } else {
            // TODO: Log error
        }
    }
    send_response(argv[0], NULL, MSG_OK);
    return ESP_OK;
}

esp_err_t cmd_interactive(int argc, char **argv) {
    /* Enable or disable logging based on status of Interactive Mode */
    ActionType action = parseCommand(argc, argv);
    switch (action) {
        case ACTION_ENABLE:
            wendigo_set_logging(ESP_LOG_INFO);
            linenoiseSetDumbMode(0);
            ESP_LOGI(TAG, "Enabling Interactive Mode...Done.");
            break;
        case ACTION_DISABLE:
            linenoiseSetDumbMode(1);
            ESP_LOGI(TAG, "Disabling Interactive Mode...Done.");
            wendigo_set_logging(ESP_LOG_NONE);
            break;
        default:
            /* Do nothing for status */
            break;
    }
    enableDisableRadios(argc, argv, SCAN_INTERACTIVE, NULL, NULL);
    return ESP_OK;
}

/** Usage: t[ag] ( b[t] | w[ifi] ) <MAC> [ 0 | 1 | 2 ] */
esp_err_t cmd_tag(int argc, char **argv) {
    esp_err_t result = ESP_OK;
    esp_bd_addr_t addr;
    send_response(argv[0], argv[1], MSG_ACK);
    ActionType action = parse_command_tag(argc, argv, addr);
    if (action == ACTION_INVALID) {
        invalid_command(argv[0], argv[1], syntaxTip[SCAN_TAG]);
        result = ESP_ERR_INVALID_ARG;
    }
    if (result == ESP_OK) {
        /* Fetch the specified device */
        wendigo_device *device = retrieve_by_mac(addr);
        if (device == NULL) {
            if (scanStatus[SCAN_INTERACTIVE] == ACTION_ENABLE) {
                ESP_LOGE(TAG, "Failed to retrieve device %s\n", argv[2]);
            }
            invalid_command(argv[0], argv[1], syntaxTip[SCAN_TAG]);
            result = ESP_ERR_INVALID_ARG;
        }
        if (result == ESP_OK) {
            switch (action) {
                case ACTION_DISABLE:
                    device->tagged = false;
                    if (scanStatus[SCAN_INTERACTIVE] == ACTION_ENABLE) {
                        ESP_LOGI(TAG, "Device %s untagged", argv[2]);
                    }
                    break;
                case ACTION_ENABLE:
                    device->tagged = true;
                    if (scanStatus[SCAN_INTERACTIVE] == ACTION_ENABLE) {
                        ESP_LOGI(TAG, "Device %s tagged", argv[2]);
                    }
                    break;
                case ACTION_STATUS:
                    if (scanStatus[SCAN_INTERACTIVE] == ACTION_ENABLE) {
                        ESP_LOGI(TAG, "Device %s IS %stagged.", argv[2], (device->tagged)?"":"NOT ");
                    }
                    break;
                default:
                    invalid_command(argv[0], argv[1], syntaxTip[SCAN_TAG]);
                    result = ESP_ERR_INVALID_ARG;
                    break;
            }
        }
    }
    if (result == ESP_OK) {
        send_response(argv[0], argv[1], MSG_OK);
    } else {
        send_response(argv[0], argv[1], MSG_FAIL);
    }
    return ESP_OK;
}

esp_err_t cmd_focus(int argc, char **argv) {
    esp_err_t result = ESP_OK;
    ActionType action = parseCommand(argc, argv);
    if (action == ACTION_INVALID) {
        invalid_command(argv[0], argv[1], syntaxTip[SCAN_FOCUS]);
        return ESP_ERR_INVALID_ARG;
    }
    send_response(argv[0], argv[1], MSG_ACK);
    switch (action) {
        case ACTION_DISABLE:
            scanStatus[SCAN_FOCUS] = ACTION_DISABLE;
            /* Everything else should take care of itself */
            break;
        case ACTION_ENABLE:
            scanStatus[SCAN_FOCUS] = ACTION_ENABLE;
            break;
        case ACTION_STATUS:
            if (scanStatus[SCAN_INTERACTIVE] == ACTION_ENABLE) {
                ESP_LOGI(TAG, "Focus Mode: %s\n", (scanStatus[SCAN_FOCUS] == ACTION_ENABLE)?STRING_ACTIVE:STRING_IDLE);
            } else {
                printf("%s status %d\n", radioShortNames[SCAN_FOCUS], scanStatus[SCAN_FOCUS]);
            }
            break;
        default:
            invalid_command(argv[0], argv[1], syntaxTip[SCAN_FOCUS]);
            result = ESP_ERR_INVALID_ARG;
    }
    if (result == ESP_OK && action != ACTION_STATUS) {
        if (scanStatus[SCAN_INTERACTIVE] == ACTION_ENABLE) {
            ESP_LOGI(TAG, "Focus Mode %s successfully.", (action == ACTION_ENABLE)?"enabled":(action == ACTION_DISABLE)?"disabled":"queried");
        } else {
            send_response(argv[0], argv[1], MSG_OK);
        }
    } else if (action != ACTION_STATUS) {
        if (scanStatus[SCAN_INTERACTIVE] == ACTION_ENABLE) {
            ESP_LOGE(TAG, "Failed to %s Focus Mode.", (action == ACTION_ENABLE)?"enable":(action == ACTION_DISABLE)?"disable":"query");
        } else {
            send_response(argv[0], argv[1], MSG_FAIL);
        }
    }
    return result;
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
    repl_config.prompt = "";
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
    
    /* Create the UART mutex */
    uartMutex = xSemaphoreCreateMutex();

    #if defined(CONFIG_ESP_CONSOLE_UART_DEFAULT) || defined(CONFIG_ESP_CONSOLE_UART_CUSTOM)
        esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));
        /* Terminate lines with LF instead of the default CRLF */
        uart_vfs_dev_port_set_tx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_LF);
    #elif defined(CONFIG_ESP_CONSOLE_USB_CDC)
        esp_console_dev_usb_cdc_config_t hw_config = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_console_new_repl_usb_cdc(&hw_config, &repl_config, &repl));

    #elif defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
        esp_console_dev_usb_serial_jtag_config_t hw_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &repl));

    #else
        #error Unsupported console type
    #endif
    /* Disable command completion and hints unless in Interactive Mode */
    linenoiseSetDumbMode(1);
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
