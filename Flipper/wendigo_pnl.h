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

uint16_t index_of_pnl(char *ssid);
PreferredNetwork *pnl_for_ssid(char *ssid);
uint8_t count_networks_for_device(wendigo_device *dev);
uint16_t map_ssids_to_devices(WendigoApp *app);
uint8_t get_networks_for_device(WendigoApp *app, wendigo_device *dev, char ***result);
uint16_t get_all_networks(WendigoApp *app);

/* Preferred Network List caches */
extern PreferredNetwork *networks;
extern uint16_t networks_count;
extern uint16_t networks_capacity;

#endif