#pragma once

#include "wendigo_app_i.h"
#include <sys/time.h>

#include "wendigo_packet_offsets.h"
/* ********** MUST REMAIN IN SYNC with counterparts in ESP32-Wendigo (bluetooth.h) ********** */

/* ********** End of shared objects ********** */

/* Encapsulating struct so I have somewhere to store the class of device string */
typedef struct {
    wendigo_bt_device dev;
    char *cod_str;
    VariableItem *view;
} flipper_bt_device;

/* Function imports from scenes */
extern void wendigo_scene_device_list_update(WendigoApp *app, flipper_bt_device *dev);
extern void wendigo_scene_status_add_attribute(WendigoApp *app, char *name, char *value);
extern void wendigo_scene_status_finish_layout(WendigoApp *app);
extern void wendigo_scene_status_begin_layout(WendigoApp *app);

/* Device caches - Declared extern to get around header spaghetti */
extern flipper_bt_device **bt_devices;
extern flipper_bt_device **bt_selected_devices;
extern uint16_t bt_devices_count;
extern uint16_t bt_selected_devices_count;
extern uint16_t bt_devices_capacity;
extern uint16_t bt_selected_devices_capacity;
// TODO: WiFi

void wendigo_set_scanning_active(WendigoApp *app, bool starting);
void wendigo_scan_handle_rx_data_cb(uint8_t* buf, size_t len, void* context);
void wendigo_free_uart_buffer();
void wendigo_esp_version(WendigoApp *app);
void wendigo_esp_status(WendigoApp *app);
void wendigo_free_bt_devices();
bool wendigo_set_bt_device_selected(flipper_bt_device *device, bool selected);
uint16_t bt_custom_device_index(flipper_bt_device *dev, flipper_bt_device **array, uint16_t array_count);
bool wendigo_update_bt_device(WendigoApp *app, flipper_bt_device *dev);
bool wendigo_add_bt_device(WendigoApp *app, flipper_bt_device *dev);
