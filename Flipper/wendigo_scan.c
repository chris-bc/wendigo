#include "wendigo_scan.h"

/* Enable or disable Wendigo scanning on all interfaces, using app->interfaces
   to determine which radios should be enabled/disabled when starting to scan.
   This function is called by the UI handler (wendigo_scene_start) when scanning
   is started or stopped. app->interfaces is updated via the Setup menu.
   Because wendigo_uart_tx returns void there is no way to identify failure,
   so this function also returns void.
*/
void wendigo_set_scanning_active(WendigoApp *app, bool starting) {
    char cmd;
    uint8_t arg;
    const uint8_t CMD_LEN = 5; // e.g. "b 1\n\0"
    char cmdString[CMD_LEN];
    for (int i = 0; i < IF_COUNT; ++i) {
        /* Set command */
        cmd = (i == IF_BLE) ? 'b' : (i == IF_BT_CLASSIC) ? 'h' : 'w';
        /* arg */
        arg = (starting && app->interfaces[i].active) ? 1 : 0;
        snprintf(cmdString, CMD_LEN, "%c %d\n", cmd, arg);
        wendigo_uart_tx(app->uart, (uint8_t *)cmdString, CMD_LEN);
    }
    app->is_scanning = starting;
}
