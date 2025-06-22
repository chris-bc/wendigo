#ifndef WENDIGO_WIFI_H
#define WENDIGO_WIFI_H

#include "common.h"

static const char *WIFI_TAG = "WiFi@Wendigo";

esp_err_t initialise_wifi();
void wifi_pkt_rcvd(void *buf, wifi_promiscuous_pkt_type_t type);
esp_err_t wendigo_wifi_disable();
esp_err_t wendigo_wifi_enable();
esp_err_t wendigo_get_channels();
esp_err_t wendigo_set_channels(uint8_t *new_channels, uint8_t new_channels_count);
bool wendigo_is_valid_channel(uint8_t channel);

/* Offsets for different packet types */
uint8_t BEACON_SSID_OFFSET = 38;
uint8_t BEACON_SRCADDR_OFFSET = 10;
uint8_t BEACON_DESTADDR_OFFSET = 4;
uint8_t BEACON_BSSID_OFFSET = 16;
uint8_t BEACON_SEQNUM_OFFSET = 22;
uint8_t BEACON_PRIVACY_OFFSET = 34; /* 0x31 set, 0x21 unset */
uint8_t BEACON_PACKET_LEN = 57;
uint8_t DEAUTH_DESTADDR_OFFSET = 4;
uint8_t DEAUTH_SRCADDR_OFFSET = 10;
uint8_t DEAUTH_BSSID_OFFSET = 16;
uint8_t PROBE_SSID_OFFSET = 26;
uint8_t PROBE_SRCADDR_OFFSET = 10;
uint8_t PROBE_DESTADDR_OFFSET = 4;
uint8_t PROBE_BSSID_OFFSET = 16;
uint8_t PROBE_SEQNUM_OFFSET = 22;
uint8_t PROBE_REQUEST_LEN = 42;
uint8_t PROBE_RESPONSE_DESTADDR_OFFSET = 4;
uint8_t PROBE_RESPONSE_SRCADDR_OFFSET = 10;
uint8_t PROBE_RESPONSE_BSSID_OFFSET = 16;
uint8_t PROBE_RESPONSE_PRIVACY_OFFSET = 34;
uint8_t PROBE_RESPONSE_SSID_OFFSET = 38;
uint8_t PROBE_RESPONSE_GROUP_CIPHER_OFFSET = 62; /* + ssid_len */
uint8_t PROBE_RESPONSE_PAIRWISE_CIPHER_OFFSET = 68; /* + ssid_len */
uint8_t PROBE_RESPONSE_AUTH_TYPE_OFFSET = 74; /* + ssid_len */
uint8_t PROBE_RESPONSE_LEN = 173;
uint8_t RTS_CTS_DESTADDR = 4;
uint8_t RTS_CTS_SRCADDR = 10;

typedef enum WiFi_Frame {
    WIFI_FRAME_ASSOC_REQ = 0x00,
    WIFI_FRAME_ASSOC_RESP = 0x10,
    WIFI_FRAME_REASSOC_REQ = 0x20,
    WIFI_FRAME_REASSOC_RESP = 0x30,
    WIFI_FRAME_PROBE_REQ = 0x40,
    WIFI_FRAME_PROBE_RESP = 0x50,
    WIFI_FRAME_BEACON = 0x80,
    WIFI_FRAME_ATIMS = 0x90,
    WIFI_FRAME_DISASSOC = 0xa0,
    WIFI_FRAME_AUTH = 0xb0,
    WIFI_FRAME_DEAUTH = 0xc0,
    WIFI_FRAME_ACTION = 0xd0,
    WIFI_FRAME_RTS = 0xB4,
    WIFI_FRAME_CTS = 0xC4,
    WIFI_FRAME_DATA = 0x08,
    WIFI_FRAME_DATA_ALT = 0x88,
    WIFI_FRAME_COUNT = 16
} WiFi_Frame;

typedef enum PROBE_RESPONSE_AUTH_TYPE {
    AUTH_TYPE_NONE = 1,
    AUTH_TYPE_WEP = 2,
    AUTH_TYPE_WPA = 4
} PROBE_RESPONSE_AUTH_TYPE;

#endif