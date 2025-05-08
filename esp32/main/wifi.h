#ifndef WENDIGO_WIFI_H
#define WENDIGO_WIFI_H

#include "common.h"

esp_err_t initialise_wifi();
void wifi_pkt_rcvd(void *buf, wifi_promiscuous_pkt_type_t type);
esp_err_t wendigo_wifi_disable();
esp_err_t wendigo_wifi_enable();

#endif