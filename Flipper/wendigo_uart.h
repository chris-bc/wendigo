#pragma once

#include "furi_hal.h"

#define RX_BUF_SIZE (320)

typedef struct Wendigo_Uart Wendigo_Uart;

void wendigo_uart_set_handle_rx_data_cb(
    Wendigo_Uart* uart,
    void (*handle_rx_data_cb)(uint8_t* buf, size_t len, void* context));
void wendigo_uart_tx(Wendigo_Uart* uart, uint8_t* data, size_t len);
Wendigo_Uart* wendigo_uart_init(WendigoApp* app);
void wendigo_uart_free(Wendigo_Uart* uart);
