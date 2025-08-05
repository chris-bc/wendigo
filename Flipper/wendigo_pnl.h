#ifndef WENDIGO_PNL_H
#define WENDIGO_PNL_H

/** PreferredNetwork is used to allow us to browse from SSID to devices
 * that have probed for that SSID.
 */
typedef struct PreferredNetwork {
    char ssid[MAX_SSID_LEN + 1];
    uint8_t device_count;
    wendigo_device **devices;
} PreferredNetwork;

/* Preferred Network List caches */
extern PreferredNetwork *networks;
extern uint16_t networks_count;
extern uint16_t networks_capacity;

#endif