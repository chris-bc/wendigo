#pragma once

#include "wendigo_app_i.h"

/* Function imports from scenes */
extern void wendigo_scene_device_list_update(WendigoApp *app, wendigo_device *dev);
extern void wendigo_scene_status_add_attribute(WendigoApp *app, char *name, char *value);
extern void wendigo_scene_status_finish_layout(WendigoApp *app);
extern void wendigo_scene_status_begin_layout(WendigoApp *app);

/* Device caches - Declared extern to get around header spaghetti */
extern wendigo_device **devices;
extern wendigo_device **selected_devices;
extern uint16_t devices_count;
extern uint16_t selected_devices_count;
extern uint16_t devices_capacity;
extern uint16_t selected_devices_capacity;

void wendigo_set_scanning_active(WendigoApp *app, bool starting);
void wendigo_scan_handle_rx_data_cb(uint8_t* buf, size_t len, void* context);
void wendigo_free_uart_buffer();
void wendigo_version(WendigoApp *app);
void wendigo_esp_status(WendigoApp *app);
void wendigo_free_devices();
bool wendigo_set_device_selected(wendigo_device *device, bool selected);
uint16_t bt_custom_device_index(wendigo_device *dev, wendigo_device **array, uint16_t array_count);
bool wendigo_update_device(WendigoApp *app, wendigo_device *dev);
bool wendigo_add_device(WendigoApp *app, wendigo_device *dev);
void wendigo_log(MsgType logType, char *message);
void wendigo_log_with_packet(MsgType logType, char *message, uint8_t *packet, uint16_t packet_size);