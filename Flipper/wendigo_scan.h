#pragma once

#include "wendigo_app_i.h"

void wendigo_set_scanning_active(WendigoApp *app, bool starting);
void wendigo_scan_handle_rx_data_cb(uint8_t* buf, size_t len, void* context);
void wendigo_free_uart_buffer();
