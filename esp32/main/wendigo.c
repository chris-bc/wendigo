/* Basic console example (esp_console_repl API)

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "wendigo.h"
#include "common.h"
#include "wifi.h"

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

/* Display command syntax when in interactive mode */
void display_syntax(char *command) {
    printf("Usage: %s ( 0 | 1 | 2 )\n0: Disable\n1: Enable\n2: Status\n", command);
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

/* Return the specified response over UART */
esp_err_t send_response(char *cmd, char *arg, MsgType result) {
    char resultMsg[8];
    switch (result) {
        case MSG_ACK:
            strcpy(resultMsg, "ACK");
            break;
        case MSG_OK:
            strcpy(resultMsg, "OK");
            break;
        case MSG_FAIL:
            strcpy(resultMsg, "FAIL");
            break;
        case MSG_INVALID:
            strcpy(resultMsg, "INVALID");
            break;
        default:
            ESP_LOGE(TAG, "Invalid result type: %d\n", result);
            return ESP_ERR_INVALID_ARG;
    }
    if (arg == NULL) {
        printf("%s %s\n", cmd, resultMsg);
    } else {
        printf("%s %s %s\n", cmd, arg, resultMsg);
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

esp_err_t cmd_bluetooth(int argc, char **argv) {
    //
    return ESP_OK;
}

esp_err_t cmd_ble(int argc, char **argv) {
    //
    return ESP_OK;
}

esp_err_t cmd_wifi(int argc, char **argv) {
    esp_err_t err = ESP_OK;

    ActionType action = parseCommand(argc, argv);
    if (action == ACTION_INVALID) {
        invalid_command(argv[0], argv[1], syntaxTip[SCAN_WIFI]);
        err = ESP_ERR_INVALID_ARG;
    } else {
        /* Acknowledge the message */
        send_response(argv[0], argv[1], MSG_ACK);
        /* Perform the command */
        switch (action) {
            case ACTION_DISABLE:
                if (scanStatus[SCAN_WIFI]) {
                    scanStatus[SCAN_WIFI] = false;
                    err = wendigo_wifi_disable();
                }
                break;
            case ACTION_ENABLE:
                if (!scanStatus[SCAN_WIFI]) {
                    scanStatus[SCAN_WIFI] = true;
                    err = wendigo_wifi_enable();
                }
                break;
            case ACTION_STATUS:
                if (scanStatus[SCAN_INTERACTIVE] == ACTION_ENABLE) {
                    ESP_LOGI(TAG, "WiFi Scanning %s\n", (scanStatus[SCAN_WIFI] == ACTION_ENABLE)?"Enabled":"Disabled");
                } else {
                    printf("wifi status %d\n", scanStatus[SCAN_WIFI]);
                }
                break;
            default:
                invalid_command(argv[0], argv[1], syntaxTip[SCAN_WIFI]);
                err = ESP_ERR_INVALID_ARG;
                break;
        }
    }
    if (err == ESP_OK) {
        /* Command succeeded. Inform success */
        send_response(argv[0], argv[1], MSG_OK);
    } else {
        /* Command failed */
        send_response(argv[0], argv[1], MSG_FAIL);
    }
    return ESP_OK;
}

esp_err_t cmd_status(int argc, char **argv) {
    //
    return ESP_OK;
}

esp_err_t cmd_version(int argc, char **argv) {
    //
    return ESP_OK;
}

esp_err_t cmd_interactive(int argc, char **argv) {
    //
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
#endif // CONFIG_STORE_HISTORY

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
