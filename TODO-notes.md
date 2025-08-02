REMOVED * Fix interactive mode display bug by only updating lastSeen if the required period has elapsed - Kludgey, but it'll probably never be used anyway - And Flipper creates its own timestamp so it won't affect Flipper-Wendigo

* Check whether 802.11 data packets can be sent between stations - Currently the parser assumes that a STA will only be sending a data packet to an AP

* wendigo_add_device()
  * Acquire device mutex when realloc()ing and when appending
  * Release when devices[] has been set to the result of realloc(), and count has been incremented to avoid 2 devices being put in the same element

* variable_item_list_set_header
  * If NULL calls furi_string_reset() to reset it
  * Otherwise uses furi_string_set_str to update it
    * That must be buggy, so NULL the header in _on_enter()
    * Done for device_list, pnl_list, setup_channel, setup, start & status

#### wendigo_scene_device_list.c
* Ensure current_devices is sound
* Update wendigo_device_is_displayed() to support DEVICE_CUSTOM

#### Manage current_devices.devices[] - free or realloc
* Often doesn't respect free_devices, but I think it's only used when setting devices to a PNL element, and I'm pretty sure devices[] is copied to a newly-allocated location for that.
  * Either change that so PNL elements are used in-place and free_devices is respected, or remove free_devices from the model.

### Built-in functions
* furi_string_alloc()
* furi_string_free(theStr)
* const char *furi_string_get_cstr(theStr)
* furi_string_cat(str1, str2)
* furi_string_cat_str(str1, char *str2)
* furi_string_search(strPacket, strNeedle, startIdx)
* furi_string_search_str(strPacket, char *needle, startIdx)
* furi_string_left(theStr, nBytes)

### Mutex - base.h thread.h
* FuriMutex *furi_mutex_alloc(FuriMutexType type) => * FuriMutexTypeNormal, FuriMutexTypeRecursive
* furi_mutex_free(theMutex)
* FuriStatus furi_mutex_acquire(theMutex, uint32_t timeout) => 0xffffffff?
* FuriStatus furi_mutex_release(theMutex)
* FuriStatusOk is an important status
* FuriWaitForever is a good timeout

```c
typedef struct DeviceListInstance {
  wendigo_device **devices;
  uint16_t devices_count;
  uint8_t devices_mask;
  WendigoAppView view;
  char devices_msg[MAX_SSID_LEN + 18];
  bool free_devices;
} DeviceListInstance;
```

### For 5GHz WiFi
* enum wifi_2g_channel_bit_t
* enum wifi_5g_channel_bit_t

