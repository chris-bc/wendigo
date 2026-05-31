#ifndef WENDIGO_WIFI_H
#define WENDIGO_WIFI_H

#include "common.h"

esp_err_t initialise_wifi();
void wifi_pkt_rcvd(void *buf, wifi_promiscuous_pkt_type_t type);
esp_err_t wendigo_wifi_disable();
esp_err_t wendigo_wifi_enable();
esp_err_t wendigo_get_channels();
esp_err_t wendigo_set_channels(uint8_t *new_channels, uint8_t new_channels_count);
uint8_t wendigo_add_channels(uint8_t *new_channels, uint8_t new_channels_count);
uint8_t wendigo_rm_channels(uint8_t *old_channels, uint8_t old_channels_count);
esp_err_t wendigo_reset_channels();
bool wendigo_is_valid_channel(uint8_t channel);

/* Offsets for different packet types */
uint8_t BEACON_SSID_OFFSET = 38;
uint8_t BEACON_SEQNUM_OFFSET = 22;
uint8_t BEACON_PRIVACY_OFFSET = 34; /* 0x31 set, 0x21 unset ... Or is it 0x01 open 0x11 private?*/
uint8_t BEACON_PRIVACY_OFF = 0x01;
uint8_t BEACON_PRIVACY_ON = 0x11;
uint8_t BEACON_TAGS_OFFSET = 36; /* Variable-length tags incl. SSID, channel, security begin here */
const uint8_t BEACON_TAG_SSID = 0x00;
const uint8_t BEACON_TAG_CHANNEL = 0x03;
const uint8_t BEACON_TAG_WPA1 = 0xdd;
const uint8_t BEACON_WPA1_TYPE[] = {0x00, 0x50, 0xF2, 0x01};
const uint8_t BEACON_TAG_WPA2 = 0x30;
uint8_t BEACON_PACKET_LEN = 57;
uint8_t PROBE_SSID_OFFSET = 26;
uint8_t PROBE_SEQNUM_OFFSET = 22;
uint8_t PROBE_REQUEST_LEN = 42;
uint8_t PROBE_RESPONSE_PRIVACY_OFFSET = 34; /* On {0x11, 0x11} Off {0x01, 0x11}*/
uint8_t PROBE_RESPONSE_SSID_OFFSET = 38;
uint8_t PROBE_RESPONSE_GROUP_CIPHER_OFFSET = 62; /* + ssid_len */
uint8_t PROBE_RESPONSE_PAIRWISE_CIPHER_OFFSET = 68; /* + ssid_len */
uint8_t PROBE_RESPONSE_AUTH_TYPE_OFFSET = 74; /* + ssid_len | PROBE_RESPONSE_AUTH_TYPE */
uint8_t PROBE_RESPONSE_LEN = 173;
uint8_t DESTADDR_80211_OFFSET = 4; /* Generic 802.11 packet offsets */
uint8_t SRCADDR_80211_OFFSET = 10;
uint8_t BSSID_80211_OFFSET = 16;

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

#endif