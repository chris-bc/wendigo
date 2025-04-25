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
 * ESP32 Wendigo: https://github.com/chris-bc/wendigo   *
 * Licensed under the MIT Open Source License.                *
 *************************************************************/

#include "wendigo.h"

#include "common.h"
#include "esp_err.h"
#include "esp_wifi_types.h"
#include "freertos/portmacro.h"
#include "sdkconfig.h"
#define PROMPT_STR "wendigo"
#define TAG "WENDIGO"

/* Console command history can be stored to and loaded from a file.
 * The easiest way to do this is to use FATFS filesystem on top of
 * wear_levelling library.
 */
#if CONFIG_CONSOLE_STORE_HISTORY

#define MOUNT_PATH "/data"
#define HISTORY_PATH MOUNT_PATH "/history.txt"

/* Over-ride the default implementation so we can send deauth frames */
esp_err_t ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2, int32_t arg3){
    return ESP_OK;
}

/* Display a simple out of memory message and set error code */
esp_err_t outOfMemory() {
    printf("Out of memory");
    return ESP_ERR_NO_MEM;
}

/* Return the specified response over UART */
esp_err_t send_response(char *cmd, char *arg, MsgType result) {
    char resultMsg[5];
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

/* Display command syntax for manipulating the HCI interface */
void display_bt_syntax() {
    printf("Usage: H[CI] ( 0 | 1 | 2 )\n0: Disable\n1: Enable\n2: Status\n");
}

/* This command is used to configure Bluetooth Classic status */
esp_err_t cmd_bluetooth(int argc, char **argv) {
    esp_err_t err = ESP_OK;
    #ifndef CONFIG_BT_ENABLED
        ESP_LOGE(TAG, "Bluetooth Unsupported");
        return ESP_ERR_NOT_ALLOWED;
    #endif

    if (argc == 2 && strlen(argv[1]) == 1) {
        /* Acknowledge the message */
        send_response(argv[0], argv[1], MSG_ACK);

        /* Perform the command */
        /* First verify syntax - argv[1] is '0', '1', or '2' - Convert to ActionType */
        switch (strtol(argv[1], NULL, 10)) {
            case ACTION_DISABLE:
                // TODO: err = disable_bt();
                break;
            case ACTION_ENABLE:
                // TODO: err = enable_bt();
                break;
            case ACTION_STATUS:
                // TODO: err = bt_status();
                break;
            default:
                display_bt_syntax();
                err = ESP_ERR_INVALID_ARG;
                break;
        }
    } else {
        display_bt_syntax();
        err = ESP_ERR_INVALID_ARG;
    }
    if (err == ESP_OK) {
        /* Command succeeded. Inform success */
        send_response(argv[0], argv[1], MSG_OK);
    } else {
        /* Command failed */
        send_response(argv[0], (argc == 2)?argv[1]:NULL, MSG_FAIL);
    }
    return ESP_OK;
}

/* Configure BLE status */
esp_err_t cmd_ble(int argc, char **argv) {
    esp_err_t err = ESP_OK;
    // CONFIG_BLE??
    // TODO: Enable/Disable BLE scanning
    return err;
}

esp_err_t cmd_wifi(int argc, char **agrv) {
    esp_err_t err = ESP_OK;
    // TODO: Manipulate WiFi
    return err;
}

esp_err_t cmd_status(int argc, char **argv) {
    esp_err_t err = ESP_OK;
    // TODO: Do stuff
    return err;
}

/* Display version info for esp32-Wendigo */
esp_err_t cmd_version(int argc, char **argv) {
    esp_err_t err = ESP_OK;

    #ifdef CONFIG_FLIPPER
        printf("esp32-Wendigo v%s\n", WENDIGO_VERSION);
    #else
        ESP_LOGI(TAG, "esp32-Wendigo v%s\n", WENDIGO_VERSION);
    #endif
    return err;
}

/* Monitor mode callback
   This is the callback function invoked when the wireless interface receives any selected packet.
   Currently it only responds to management packets.
   This function:
    - Displays packet info when sniffing is enabled
    - Coordinates Mana probe responses when Mana is enabled
    - Invokes relevant functions to manage scan results, if scanning is enabled
*/
void wifi_pkt_rcvd(void *buf, wifi_promiscuous_pkt_type_t type) {
    wifi_promiscuous_pkt_t *data = (wifi_promiscuous_pkt_t *)buf;
UNUSED(data);
    //ESP_LOGE("UNIMPLEMENTED", "wifi_pkt_rcvd is not yet implemented, I don't know what to do with this packet but its RSSI is %d\n", data->rx_ctrl.rssi);
  
    return;
}

int initialise_wifi() {
    /* Initialise WiFi if needed */
    if (!WIFI_INITIALISED) {
        esp_err_t ret = nvs_flash_init();
        if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            ESP_ERROR_CHECK(nvs_flash_erase());
            ret = nvs_flash_init();
        }
        ESP_ERROR_CHECK(ret);

        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        esp_netif_create_default_wifi_ap();

        // Set up promiscuous mode and packet callback
        initPromiscuous();
        WIFI_INITIALISED = true;
    }
    return ESP_OK;
}

void initPromiscuous() {
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    /* Init dummy AP to specify a channel and get WiFi hardware into a
           mode where we can send the actual fake beacon frames. */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    wifi_config_t ap_config = {
        .ap = {
            .ssid = "ManagementAP",
            .ssid_len = 12,
            .password = "management",
            .channel = 1,
            .authmode = WIFI_AUTH_OPEN,
            .ssid_hidden = 0,
            .max_connection = 128,
            .beacon_interval = 5000
        }
    };

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    wifi_promiscuous_filter_t filter = { .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_CTRL | WIFI_PROMIS_FILTER_MASK_DATA };
    esp_wifi_set_promiscuous_filter(&filter);
    esp_wifi_set_promiscuous_rx_cb(wifi_pkt_rcvd);
    esp_wifi_set_promiscuous(true);
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
            #ifndef CONFIG_FLIPPER
                ESP_LOGI(TAG, "Registered command \"%s\"...", commands[i].command);
            #endif
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

    esp_log_level_set("wifi", ESP_LOG_ERROR); /* YAGNI: Consider reducing these to ESP_LOG_WARN */
    esp_log_level_set("esp_netif_lwip", ESP_LOG_ERROR);

#if CONFIG_CONSOLE_STORE_HISTORY
    initialize_filesystem();
    repl_config.history_save_path = HISTORY_PATH;
    #ifndef CONFIG_FLIPPER
        ESP_LOGI(TAG, "Command history enabled");
    #endif
#else
    #ifndef CONFIG_FLIPPER
        ESP_LOGI(TAG, "Command history disabled");
    #endif
#endif

    initialise_wifi();
    /* Register commands */
    esp_console_register_help_command();
    register_system();
    register_wifi();
    register_wendigo_commands();
    /*register_nvs();*/

    ESP_LOGI(TAG, "Started Wendigo v%s\n", WENDIGO_VERSION);

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
