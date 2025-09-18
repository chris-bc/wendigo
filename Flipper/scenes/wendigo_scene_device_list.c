#include "../wendigo_app_i.h"
#include "../wendigo_scan.h"

/** Public method from wendigo_scene_device_detail.c */
extern void wendigo_scene_device_detail_set_device(wendigo_device *d);
/** Public method from wendigo_scene_pnl_list.c */
extern void wendigo_scene_pnl_list_set_device(wendigo_device *d, WendigoApp *app);
/** Internal method - I don't wan't to move all calling functions below it */
static void wendigo_scene_device_list_var_list_change_callback(VariableItem *item);
bool wendigo_selected_options_init(DeviceListInstance *deviceList);
double _elapsedTime(uint32_t *from, uint32_t *to, char *elapsedStr, uint8_t strlen);
void wendigo_scene_device_list_update_device(VariableItem *new_item);

/** TODO: For some obscene reason the ifndef barrier isn't stopping these
 *  from showing up in every single object file. No longer shared.
 */
char *wifi_auth_mode_strings[] = {"Open", "WEP", "WPA", "WPA2",
    "WPA+WPA2", "EAP", "EAP", "WPA3", "WPA2+WPA3", "WAPI", "OWE",
    "WPA3 Enterprise 192-bit", "WPA3 EXT", "WPA3 EXT Mixed Mode", "DPP",
    "WPA3 Enterprise", "WPA3 Enterprise Transition", "Unknown"};

FuriTimer *deviceTimer = NULL;

/** Frequency that device attributes are updated */
#define DEVICE_REFRESH_MS (300)

/** Enum to index the options menu for devices */
enum wendigo_device_list_bt_options {
  WendigoOptionBTRSSI = 0,
  WendigoOptionBTTagUntag,
  WendigoOptionBTScanType,
  WendigoOptionBTCod,
  WendigoOptionBTLastSeen,
  WendigoOptionsBTCount
};

enum wendigo_device_list_ap_options {
  WendigoOptionAPRSSI = 0,
  WendigoOptionAPTagUntag,
  WendigoOptionAPScanType,
  WendigoOptionAPStaCount,
  WendigoOptionAPAuthMode,
  WendigoOptionAPChannel,
  WendigoOptionAPLastSeen,
  WendigoOptionsAPCount
};

enum wendigo_device_list_sta_options {
  WendigoOptionSTARSSI = 0,
  WendigoOptionSTATagUntag,
  WendigoOptionSTAScanType,
  WendigoOptionSTASavedNetworks,
  WendigoOptionSTAAP,
  WendigoOptionSTAChannel,
  WendigoOptionSTALastSeen,
  WendigoOptionsSTACount
};

/* Enacting nested copies of these scenes, without the luxury of treating them
 * as instances of a class, is achieved by managing an array of
 * DeviceListInstances as a stack - pushing a new set of devices to the stack
 * and initialising a new set of devices for display, or popping the last
 * value off the stack and using them as the current devices to return to the
 * previously-displayed device list.
 * Implementation note: For consistency, current_devices is not pushed onto the
 * stack, unless and until a new device list is being displayed. The idiom here
 * is to push to the stack immediately before preparing a new instance of
 * current_devices, such that everything is in place by the time
 * wendigo_scene_device_list_on_enter() is called, and to pop from the stack
 * in wendigo_scene_device_list_on_exit(), where the existing current_devices[]
 * is also freed. */
uint8_t stack_counter = 0;
DeviceListInstance *stack = NULL;
DeviceListInstance current_devices;

/** Prepare current_devices for use. Provides initial values for the
 * current_devices struct.
 * `config` must be either NULL or a pointer to a DeviceListInstance that
 * provides initial values for current_devices. config may be freed after
 * calling - memory is allocated for  its devices if necessary - however
 * the wendigo_device instances referenced by devices[] are expected to
 * remain allocated for the duration of the scene.
 * It is not necessary to call this function more than once. If you do this
 * make sure you call wendigo_scene_device_list_free() before the second
 * call, otherwise any memory allocated to devices[] will be leaked.
 */
void wendigo_scene_device_list_init(void *config) {
  bzero(current_devices.devices_msg, sizeof(current_devices.devices_msg));
  if (config == NULL) {
    current_devices.devices = NULL;
    current_devices.selected_option_index = NULL;
    current_devices.selected_index = 0;
    current_devices.devices_count = 0;
    current_devices.devices_mask = DEVICE_ALL;
    current_devices.view = WendigoAppViewDeviceList;
    current_devices.free_devices = true;
  } else {
    DeviceListInstance *cfg = (DeviceListInstance *)config;
    if (current_devices.devices_count > 0 && current_devices.devices != NULL &&
        current_devices.free_devices) {
      free(current_devices.devices);
    }
    if (cfg->devices_count > 0 && cfg->devices != NULL) {
      current_devices.devices = malloc(sizeof(wendigo_device *) * cfg->devices_count);
      current_devices.selected_option_index = malloc(sizeof(uint8_t) * cfg->devices_count);
      if (current_devices.devices == NULL || current_devices.selected_option_index == NULL) {
        char *msg = malloc(sizeof(char) * 68);
        if (msg == NULL) {
          wendigo_log(MSG_ERROR, "Unable to allocate memory for DeviceListInstance initialiser.");
        } else {
          snprintf(msg, 68, "Unable to allocate %d bytes for DeviceListInstance initialiser.",
            (sizeof(wendigo_device *) + 1) * cfg->devices_count); /* +1 accounts for selected_option_index */
          wendigo_log(MSG_ERROR, msg);
          free(msg);
        }
        current_devices.devices_count = 0;
        /* Free either of the arrays if they were allocated (we know at least one failed - maybe not both)*/
        if (current_devices.devices != NULL) {
          free(current_devices.devices);
          current_devices.devices = NULL;
        }
        if (current_devices.selected_option_index != NULL) {
          free(current_devices.selected_option_index);
          current_devices.selected_option_index = NULL;
        }
      } else {
        memcpy(current_devices.devices, cfg->devices, sizeof(wendigo_device *) * cfg->devices_count);
        current_devices.devices_count = cfg->devices_count;
        /* Does cfg contain selected options? */
        if (cfg->selected_option_index != NULL) {
          memcpy(current_devices.selected_option_index,
            cfg->selected_option_index, sizeof(uint8_t) * cfg->devices_count);
        } else {
          /* Initialise the selected_options_index[] when it's first loaded */
          // TODO Test to verify that this is not called when returning to this scene after going to another.
          wendigo_selected_options_init(&current_devices);
        }
      }
    } else {
      current_devices.devices = NULL;
      current_devices.selected_option_index = NULL;
      current_devices.selected_index = cfg->selected_index;
      current_devices.devices_count = 0;
    }
    current_devices.devices_mask = cfg->devices_mask;
    strncpy(current_devices.devices_msg, cfg->devices_msg, sizeof(current_devices.devices_msg));
    current_devices.view = cfg->view;
    current_devices.free_devices = true; /* Because we malloc()d devices[] */
  }
}

/** Clean up current_devices and stack in preparation for application exit. */
void wendigo_scene_device_list_free() {
  /* Clear current_devices */
  if (current_devices.devices_count > 0 && current_devices.devices != NULL) {
    if (current_devices.selected_option_index != NULL) {
      free(current_devices.selected_option_index);
    }
    if (current_devices.free_devices) {
      free(current_devices.devices);
    }
    current_devices.devices = NULL;
    current_devices.selected_option_index = NULL;
    current_devices.devices_count = 0;
    current_devices.free_devices = true;
  }
  if (stack_counter > 0 && stack != NULL) {
    /* Free components of the device stack */
    for (uint8_t i = 0; i < stack_counter; ++i) {
      if (stack[i].devices_count > 0 && stack[i].devices != NULL) {
        if (stack[i].selected_option_index != NULL) {
          free(stack[i].selected_option_index);
        }
        if (stack[i].free_devices) {
          free(stack[i].devices);
        }
        stack[i].devices = NULL;
        stack[i].selected_option_index = NULL;
        stack[i].devices_count = 0;
        stack[i].free_devices = true;
      }
    }
    free(stack);
    stack = NULL;
    stack_counter = 0;
  }
}

/** This function is called periodically to update all device views on the
 * device list.
 */
void wendigo_scene_device_list_timer_callback(void *context) {
  FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_device_list_timer_callback()");
  WendigoApp *app = (WendigoApp *)context;
  if (app == NULL || current_devices.devices == NULL) {
    wendigo_log(MSG_ERROR, "End wendigo_scene_device_list_timer_callback() - NULL arguments.");
    return;
  }
  for (uint16_t i = 0; i < current_devices.devices_count; ++i) {
    if (current_devices.devices[i] != NULL &&
        current_devices.devices[i]->view != NULL) {
      wendigo_scene_device_list_update_device(current_devices.devices[i]->view);
    } else {
      // "ERROR: current_devices.devices[i] is NULL." 85 + 4
      char *msg = malloc(sizeof(char) * 90);
      if (msg == NULL) {
        wendigo_log(MSG_ERROR,
          "wendigo_scene_device_list_timer_callback() ERROR: current_devices.devices[i] is NULL.");
      } else {
        snprintf(msg, 90,
          "wendigo_scene_device_list_timer_callback() ERROR: current_devices.devices[%d] is NULL.",
          i);
        wendigo_log(MSG_ERROR, msg);
        free(msg);
      }
    }
  }
}

/** Start (or restart) the device timer with the specified duration */
bool wendigo_start_device_timer(WendigoApp *app, FuriTimer *timer, uint16_t millis) {
  if (app == NULL) {
    wendigo_log(MSG_ERROR, "End wendigo_start_device_timer() - Invalid arguments.");
    return false;
  }
  if (timer == NULL) {
    timer = furi_timer_alloc(wendigo_scene_device_list_timer_callback,
      FuriTimerTypePeriodic, app);
  }
  if (timer == NULL) {
    wendigo_log(MSG_ERROR, "End wendigo_start_device_timer() - Unable to allocate timer.");
    return false;
  }
  /* Stop the timer if it's running because we might have a new duration */
  if (furi_timer_is_running(timer) == 1) {
      furi_timer_stop(timer);
  }
  furi_timer_start(timer, furi_ms_to_ticks(millis));
  return (furi_timer_is_running(timer) == 1);
}

/** This alternative version of wendigo_device_is_displayed() is less efficient
 * than the original, but works correctly when the displayed device mask
 * includes DEVICE_CUSTOM.
 * This function is called automatically by wendigo_device_is_displayed() if
 * the current device mask includes DEVICE_CUSTOM - it is not necessary to
 * call this function independently.
 * Returns true if a device containing the specified MAC (dev->mac) is
 * displayed in the current scene.
 */
bool wendigo_device_is_displayed_custom(wendigo_device *dev) {
  if (dev == NULL || current_devices.devices == NULL) {
    return false;
  }
  /* Loop through current_devices.devices[] searching for dev */
  uint16_t idx;
  idx = custom_device_index(dev, current_devices.devices,
    current_devices.devices_count);
  return (idx < current_devices.devices_count);
}

/** Determine whether the specified device should be displayed, based on the
 * criteria provided in wendigo_scene_device_list_set_current_devices_mask().
 * This function DOES NOT consider devices that may be displayed by the
 * inclusion of the DEVICE_CUSTOM flag - it considers only dynamically-added
 * devices.
 */
bool wendigo_device_is_displayed(wendigo_device *dev) {
  FURI_LOG_T(WENDIGO_TAG, "Start wendigo_device_is_displayed()");
  if (dev == NULL) {
    wendigo_log(MSG_WARN, "wendigo_device_is_displayed(): dev is NULL");
    return false;
  }
  if ((current_devices.devices_mask & DEVICE_CUSTOM) == DEVICE_CUSTOM) {
    wendigo_log(MSG_INFO, "wendigo_device_is_displayed(): The current device mask includes DEVICE_CUSTOM, passing to wendigo_device_is_displayed_custom() for a precise answer.");
    return wendigo_device_is_displayed_custom(dev);
  }
  bool display_selected = ((current_devices.devices_mask & DEVICE_SELECTED_ONLY) == DEVICE_SELECTED_ONLY);
  FURI_LOG_T(WENDIGO_TAG, "End wendigo_device_is_displayed()");
  return ((dev->scanType == SCAN_HCI &&
          ((current_devices.devices_mask & DEVICE_BT_CLASSIC) == DEVICE_BT_CLASSIC) &&
            (!display_selected || dev->tagged)) ||
          (dev->scanType == SCAN_BLE &&
            ((current_devices.devices_mask & DEVICE_BT_LE) == DEVICE_BT_LE) &&
            (!display_selected || dev->tagged)) ||
          (dev->scanType == SCAN_WIFI_AP &&
            ((current_devices.devices_mask & DEVICE_WIFI_AP) == DEVICE_WIFI_AP) &&
            (!display_selected || dev->tagged)) ||
          (dev->scanType == SCAN_WIFI_STA &&
            ((current_devices.devices_mask & DEVICE_WIFI_STA) == DEVICE_WIFI_STA) &&
            (!display_selected || dev->tagged)));
}

/** Replace the contents of current_devices with the specified DeviceListInstance.
 * devices may be freed after calling this function as its contents are copied
 * into current_devices.
 */
void wendigo_scene_device_list_set_current_devices(DeviceListInstance *deviceList) {
  bzero(current_devices.devices_msg, sizeof(current_devices.devices_msg));
  if (deviceList == NULL) {
    /* Re-initialise current_devices */
    current_devices.view = WendigoAppViewDeviceList;
    current_devices.devices_mask = DEVICE_ALL;
    if (current_devices.devices_count > 0 && current_devices.devices != NULL) {
      if (current_devices.free_devices) {
        free(current_devices.devices);
      }
      if (current_devices.selected_option_index != NULL) {
        free(current_devices.selected_option_index);
      }
    }
    current_devices.devices = NULL;
    current_devices.selected_option_index = NULL;
    current_devices.devices_count = 0;
    current_devices.free_devices = true;
    return;
  }
  // TODO: Do I need a mutex over current_devices?
  wendigo_device **new_devices;
  uint8_t *new_selected_option_index;
  if (current_devices.free_devices) {
    new_devices = realloc(current_devices.devices,
      sizeof(wendigo_device *) * deviceList->devices_count);
    new_selected_option_index = realloc(
      current_devices.selected_option_index,
      deviceList->devices_count);
  } else {
    if (deviceList->devices_count > 0) {
      new_devices = malloc(sizeof(wendigo_device *) * deviceList->devices_count);
      new_selected_option_index = realloc(
        current_devices.selected_option_index,
        deviceList->devices_count);
    } else {
      new_devices = NULL;
      new_selected_option_index = NULL;
    }
  }
  if ((new_devices == NULL || new_selected_option_index == NULL) &&
      deviceList->devices_count > 0) {
    /* Log error but proceed if unable to allocate memory for device array */
    char *msg = malloc(sizeof(char) * 49);
    if (msg == NULL) {
      wendigo_log(MSG_ERROR, "Failed to allocate memory to store Device List.");
    } else {
      snprintf(msg, 49, "Failed to allocate %d bytes for Device List.",
        sizeof(wendigo_device *) * deviceList->devices_count);
      wendigo_log(MSG_ERROR, msg);
      free(msg);
    }
    /* If we can't resize current_devices.devices[] correctly then free it
     * so that no devices are displayed. */
    if (current_devices.devices_count > 0 && current_devices.devices != NULL) {
      current_devices.devices_count = 0;
      if (current_devices.free_devices) {
        free(current_devices.devices);
      }
      if (current_devices.selected_option_index != NULL) {
        free(current_devices.selected_option_index);
      }
      current_devices.devices = NULL;
      current_devices.selected_option_index = NULL;
    } else if (current_devices.devices_count > 0 &&
        current_devices.selected_option_index != NULL) {
      free(current_devices.selected_option_index);
      current_devices.selected_option_index = NULL;
    }
  } else {
    if (new_devices == NULL) {
      current_devices.devices_count = 0;
    } else {
      /* Copy across deviceList->devices[] */
      memcpy(new_devices, deviceList->devices,
        sizeof(wendigo_device *) * deviceList->devices_count);
      current_devices.devices_count = deviceList->devices_count;
      if (deviceList->selected_option_index != NULL) {
        /* Copy selected options across */
        memcpy(new_selected_option_index,
          deviceList->selected_option_index,
          deviceList->devices_count);
      } else {
        /* Initialise options with zeroes */
        bzero(new_selected_option_index, deviceList->devices_count);
      }
    }
    current_devices.devices = new_devices;
    current_devices.selected_option_index = new_selected_option_index;
  }
  current_devices.view = deviceList->view;
  current_devices.devices_mask = deviceList->devices_mask;
  current_devices.free_devices = true; /* I guess, because we allocated it above */
  memcpy(current_devices.devices_msg, deviceList->devices_msg, sizeof(current_devices.devices_msg));
}

/** Restrict displayed devices based on the specified filters
 * deviceMask: A bitmask of DeviceMask values. e.g. DEVICE_WIFI_AP |
 * DEVICE_WIFI_STA to display all WiFi devices; 0 to display all device types.
 * Returns the number of devices that meet the specified criteria.
 * If DEVICE_CUSTOM is included as part of the device mask this function WILL
 * NOT modify the contents of current_devices[], but will simply return the
 * number of devices currently displayed.
 */
uint16_t wendigo_scene_device_list_set_current_devices_mask(uint8_t deviceMask) {
  FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_device_list_set_current_devices_mask()");
  if (deviceMask == 0) {
    deviceMask = DEVICE_ALL;
  }
  current_devices.devices_mask = deviceMask;
  /* If custom devices are being displayed there's nothing for this function to do */
  if ((deviceMask & DEVICE_CUSTOM) == DEVICE_CUSTOM) {
    FURI_LOG_T(WENDIGO_TAG,
      "End wendigo_scene_device_list_set_current_devices_mask(): DEVICE_CUSTOM specified.");
    return current_devices.devices_count; // I'm pretty sure this is safe - correctly initialised as 0, then updated
  }
  bool display_selected = ((deviceMask & DEVICE_SELECTED_ONLY) == DEVICE_SELECTED_ONLY);
  /* To ensure we only malloc the required memory, run an initial pass to count the number of devices */
  uint16_t deviceCount = 0;
  for (uint16_t idx = 0; idx < devices_count; ++idx) {
    if ((devices[idx]->scanType == SCAN_HCI &&
          ((deviceMask & DEVICE_BT_CLASSIC) == DEVICE_BT_CLASSIC) &&
          (!display_selected || devices[idx]->tagged)) ||
        (devices[idx]->scanType == SCAN_BLE &&
          ((deviceMask & DEVICE_BT_LE) == DEVICE_BT_LE) &&
          (!display_selected || devices[idx]->tagged)) ||
        (devices[idx]->scanType == SCAN_WIFI_AP &&
          ((deviceMask & DEVICE_WIFI_AP) == DEVICE_WIFI_AP) &&
          (!display_selected || devices[idx]->tagged)) ||
        (devices[idx]->scanType == SCAN_WIFI_STA &&
          ((deviceMask & DEVICE_WIFI_STA) == DEVICE_WIFI_STA) &&
          (!display_selected || devices[idx]->tagged))) {
      ++deviceCount;
    }
  }
  if (deviceCount > 0) {
    /* Allocate new storage for current_devices.devices[] - malloc or
     * realloc depending on current_devices.free_devices */
    wendigo_device **new_devices;
    if (current_devices.free_devices) {
      new_devices = realloc(current_devices.devices,
        sizeof(wendigo_device *) * deviceCount);
    } else {
      new_devices = malloc(sizeof(wendigo_device *) * deviceCount);
    }
    if (new_devices == NULL) {
      char *msg = malloc(sizeof(char) * 104);
      if (msg == NULL) {
        wendigo_log(MSG_ERROR,
          "Unable to allocate current_devices.devices[] for new device mask, keeping devices[] unchanged.");
      } else {
        snprintf(msg, 104,
          "Unable to allocate current_devices.devices[%d] for new device mask %d, keeping devices[] unchanged.",
          deviceCount, deviceMask);
        wendigo_log(MSG_ERROR, msg);
        free(msg);
      }
      return 0;
    }
    current_devices.devices = new_devices;
  }
  current_devices.devices_count = deviceCount;
  /* Populate current_devices.devices[] */
  uint16_t current_index = 0;
  for (uint16_t i = 0; i < devices_count && current_index < deviceCount; ++i) {
    if ((devices[i]->scanType == SCAN_HCI &&
            ((deviceMask & DEVICE_BT_CLASSIC) == DEVICE_BT_CLASSIC) &&
            (!display_selected || devices[i]->tagged)) ||
          (devices[i]->scanType == SCAN_BLE &&
            ((deviceMask & DEVICE_BT_LE) == DEVICE_BT_LE) &&
            (!display_selected || devices[i]->tagged)) ||
          (devices[i]->scanType == SCAN_WIFI_AP &&
            ((deviceMask & DEVICE_WIFI_AP) == DEVICE_WIFI_AP) &&
            (!display_selected || devices[i]->tagged)) ||
          (devices[i]->scanType == SCAN_WIFI_STA &&
            ((deviceMask & DEVICE_WIFI_STA) == DEVICE_WIFI_STA) &&
            (!display_selected || devices[i]->tagged))) {
      current_devices.devices[current_index++] = devices[i];
    }
  }
  furi_assert(current_index == deviceCount);
  FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_device_list_set_current_devices_mask()");
  return deviceCount;
}

/** A more flexible version of elapsedTime() that lets us avoid running
 * furi_hal_rtc_get_timestamp(). This version is suitable for running at high
 * frequency - In tick events etc. `from` and `to` are seconds since the Unix
 * Epoch. Returns elapsed seconds and, if elapsedStr is an initialised char[]
 * and strlen > 0, places a text representation of the elapsed time in
 * elapsedStr. Returns zero and the empty string on failure.
 */
double _elapsedTime(uint32_t *from, uint32_t *to, char *elapsedStr,
                    uint8_t strlen) {
  FURI_LOG_T(WENDIGO_TAG, "Start _elapsedTime()");
  /* Validate everything before we touch it */
  if (from == NULL || to == NULL) {
    if (elapsedStr != NULL && strlen > 0) {
      elapsedStr[0] = '\0';
    }
    return 0;
  }
  uint32_t elapsed = *to - *from;
  if (elapsedStr != NULL && strlen > 0) {
    if (elapsed < 60) {
      snprintf(elapsedStr, strlen, "%lds", elapsed);
    } else {
      uint8_t minutes = elapsed / 60;
      uint8_t seconds = elapsed - (minutes * 60);
      snprintf(elapsedStr, strlen, "%2d:%02d", minutes, seconds);
    }
  }
  FURI_LOG_T(WENDIGO_TAG, "End _elapsedTime()");
  return elapsed;
}

/** Calculate the elapsed time since the specified device was last seen.
 * Returns the elapsed time as a uint32 and, if elapsedStr is not NULL,
 * in a string representation there. elapsedStr must be an initialised
 * char[] of at least 7 bytes.
 * If a string representation is not needed provide NULL for elapsedStr.
 * Returns zero and an empty string on failure.
 */
double elapsedTime(wendigo_device *dev, char *elapsedStr, uint8_t strlen) {
  FURI_LOG_T(WENDIGO_TAG, "Start elapsedTime()");
  if (dev == NULL) {
    /* Return sensible values on failure */
    if (strlen > 0 && elapsedStr != NULL) {
      elapsedStr[0] = '\0';
    }
    return 0;
  }
  uint32_t nowTime = furi_hal_rtc_get_timestamp();
  FURI_LOG_T(WENDIGO_TAG, "End elapsedTime()");
  return _elapsedTime(&(dev->lastSeen), &nowTime, elapsedStr, strlen);
}

/** Identify the device represented by the currently-selected menu item. NULL if
 * it cannot be identified.
 */
wendigo_device *wendigo_scene_device_list_selected_device() {
  FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_device_list_selected_device()");

  if (current_devices.selected_index < current_devices.devices_count &&
      current_devices.devices != NULL) {
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_device_list_selected_device()");
    return current_devices.devices[current_devices.selected_index];
  }
  /* Device not found */
  FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_device_list_selected_device()");
  return NULL;
}

/** Determine the default option index for the specified device.
 * This function uses the specified device's scanType to determine the
 * default option index for the device in the device list.
 */
uint8_t wendigo_scene_device_list_default_option(wendigo_device *dev) {
  if (dev == NULL) {
    return 0;
  }
  if (dev->scanType == SCAN_HCI || dev->scanType == SCAN_BLE) {
    return WendigoOptionBTScanType;
  }
  if (dev->scanType == SCAN_WIFI_AP) {
    return WendigoOptionAPScanType;
  }
  if (dev->scanType == SCAN_WIFI_STA) {
    return WendigoOptionSTAScanType;
  }
  /* If we reach this point we have an unknown scanType */
  char *msg = malloc(sizeof(char) * (78 + MAC_STRLEN));
  char *macStr = malloc(sizeof(char) * (MAC_STRLEN + 1));
  if (msg == NULL || macStr == NULL) {
    wendigo_log(MSG_ERROR,
      "wendigo_scene_device_list_default_option(): Device has unknowne scanType");
    if (msg != NULL) {
      free(msg);
    }
    if (macStr != NULL) {
      free(macStr);
    }
  } else {
    bytes_to_string(dev->mac, MAC_BYTES, macStr);
    snprintf(msg, 78 + MAC_STRLEN,
      "wendigo_scene_device_list_default_option(): Device %s has unknown scanType %d.",
      macStr, dev->scanType);
    wendigo_log(MSG_ERROR, msg);
    free(macStr);
    free(msg);
  }
  return 0;
}

/** Determines the number of options that should be displayed for a device */
uint8_t wendigo_scene_device_list_options_count(wendigo_device *dev) {
  uint8_t options_count;
  if (dev->scanType == SCAN_HCI || dev->scanType == SCAN_BLE) {
    options_count = WendigoOptionsBTCount;
  } else if (dev->scanType == SCAN_WIFI_AP) {
    options_count = WendigoOptionsAPCount;
  } else if (dev->scanType == SCAN_WIFI_STA) {
    options_count = WendigoOptionsSTACount;
  } else {
    options_count = 1;
  }
  return options_count;
}

/** Update the specified single device's view. This function is called from
 * the tick, on_enter and list_item_changed handlers to ensure displayed
 * information remains up to date.
 */
void wendigo_scene_device_list_update_device(VariableItem *new_item) {
  FURI_LOG_D(WENDIGO_TAG, "Start wendigo_scene_device_list_update_device()");
  /* Input validation */
  if (new_item == NULL || variable_item_get_context(new_item) == NULL) {
    wendigo_log(MSG_ERROR, "End wendigo_scene_device_list_update_device() - Invalid arguments.");
    return;
  }
  char *name;
  bool free_name = false;
  uint8_t optionIndex = 0;
  uint8_t dev_idx;
  char optionValue[MAX_SSID_LEN + 1];
  bzero(optionValue, MAX_SSID_LEN + 1); /* Null out optionValue[] */
  /* Fetch the device model from new_item's context */
  wendigo_device *dev = (wendigo_device *)variable_item_get_context(new_item);
  if (dev == NULL) {
    wendigo_log(MSG_ERROR, "wendigo_scene_device_list_update_device(): VariableItem has NULL context - Terminating!");
    return;
  }
  /* Use dev->scanType to determine the menu item's name/label */
  if ((dev->scanType == SCAN_HCI || dev->scanType == SCAN_BLE) &&
      dev->radio.bluetooth.bdname_len > 0 &&
      dev->radio.bluetooth.bdname != NULL) {
    /* Use bdname as name if it's a bluetooth device and we have a name */
    name = dev->radio.bluetooth.bdname;
  } else if (dev->scanType == SCAN_WIFI_AP &&
      dev->radio.ap.ssid[0] != '\0') {
    /* Use SSID if it's an AP and we have SSID */
    name = dev->radio.ap.ssid;
  } else {
    /* Otherwise use MAC */
    name = malloc(sizeof(char) * (MAC_STRLEN + 1));
    if (name != NULL) {
      free_name = true;
      bytes_to_string(dev->mac, MAC_BYTES, name);
    }
  }
  /* Figure out the index of the device represented by new_item in
   * current_devices.devices[] so we can refer to selected_option_index. */
  dev_idx = custom_device_index(dev, current_devices.devices,
    current_devices.devices_count);
  if (dev_idx == current_devices.devices_count ||
      current_devices.selected_option_index == NULL) {
    /* Device/Option not found - Log warning & display default option */
    char *msg = malloc(sizeof(char) * (103 + MAC_STRLEN));
    char *macStr = malloc(sizeof(char) * (MAC_STRLEN + 1));
    if (msg == NULL || macStr == NULL) {
      wendigo_log(MSG_WARN,
        "wendigo_scene_device_list_update_device(): Device not found in cache, defaulting display to scanType.");
    } else {
      bytes_to_string(dev->mac, MAC_BYTES, macStr);
      snprintf(msg, 103 + MAC_STRLEN,
        "wendigo_scene_device_list_update_device(): Device %s not found in cache, defaulting display to scanType.",
        macStr);
      wendigo_log(MSG_WARN, msg);
    }
    if (msg != NULL) {
      free(msg);
      msg = NULL;
    }
    if (macStr != NULL) {
      free(macStr);
    }
    /* Set default option */
    optionIndex = wendigo_scene_device_list_default_option(dev);
    if (current_devices.selected_option_index != NULL) {
      current_devices.selected_option_index[dev_idx] = optionIndex;
    }
  } else {
    /* Found the device. Get option index from selected_option_index[] */
    optionIndex = current_devices.selected_option_index[dev_idx];
  }

  /* Check which menu option new_item is displaying */
  if (((dev->scanType == SCAN_HCI || dev->scanType == SCAN_BLE) &&
      optionIndex == WendigoOptionBTRSSI) || (dev->scanType == SCAN_WIFI_AP &&
      optionIndex == WendigoOptionAPRSSI) || (dev->scanType == SCAN_WIFI_STA &&
      optionIndex == WendigoOptionSTARSSI)) {
    /* Update RSSI */
    snprintf(optionValue, sizeof(optionValue), "%d dB", dev->rssi);
  } else if (((dev->scanType == SCAN_HCI || dev->scanType == SCAN_BLE) &&
      optionIndex == WendigoOptionBTTagUntag) || (dev->scanType == SCAN_WIFI_AP
      && optionIndex == WendigoOptionAPTagUntag) ||
      (dev->scanType == SCAN_WIFI_STA &&
      optionIndex == WendigoOptionSTATagUntag)) {
    /* Update tag/untag */
    snprintf(optionValue, sizeof(optionValue), "%s",
      (dev->tagged) ? "Untag" : "Tag");
  } else if (((dev->scanType == SCAN_HCI || dev->scanType == SCAN_BLE) &&
      optionIndex == WendigoOptionBTScanType) || (dev->scanType == SCAN_WIFI_AP
      && optionIndex == WendigoOptionAPScanType) ||
      (dev->scanType == SCAN_WIFI_STA &&
      optionIndex == WendigoOptionSTAScanType)) {
    /* Update scanType */
    snprintf(optionValue, sizeof(optionValue), "%s",
      (dev->scanType == SCAN_HCI)         ? "BT Classic"
      : (dev->scanType == SCAN_BLE)       ? "BLE"
      : (dev->scanType == SCAN_WIFI_AP)   ? "WiFi AP"
      : (dev->scanType == SCAN_WIFI_STA)  ? "WiFi STA"
                                          : "Unknown");
  } else if (((dev->scanType == SCAN_HCI || dev->scanType == SCAN_BLE) &&
      optionIndex == WendigoOptionBTLastSeen) || (dev->scanType == SCAN_WIFI_AP
      && optionIndex == WendigoOptionAPLastSeen) ||
      (dev->scanType == SCAN_WIFI_STA &&
      optionIndex == WendigoOptionSTALastSeen)) {
    /* Update lastSeen */
    elapsedTime(dev, optionValue, sizeof(optionValue));
  } else if ((dev->scanType == SCAN_HCI || dev->scanType == SCAN_BLE) &&
      optionIndex == WendigoOptionBTCod) {
    /* Update BT class of device */
    strncpy(optionValue, dev->radio.bluetooth.cod_str, sizeof(optionValue));
  } else if ((dev->scanType == SCAN_WIFI_AP &&
      optionIndex == WendigoOptionAPChannel) ||
      (dev->scanType == SCAN_WIFI_STA &&
      optionIndex == WendigoOptionSTAChannel)) {
    /* Update channel */
    snprintf(optionValue, sizeof(optionValue), "Ch. %d",
      (dev->scanType == SCAN_WIFI_AP) ? dev->radio.ap.channel
                                      : dev->radio.sta.channel);
  } else if (dev->scanType == SCAN_WIFI_AP &&
      optionIndex == WendigoOptionAPStaCount) {
    /* Update AP's connected stations */
    snprintf(optionValue, sizeof(optionValue), "%d Station%s",
      dev->radio.ap.stations_count,
      (dev->radio.ap.stations_count == 1) ? "" : "s");
  } else if (dev->scanType == SCAN_WIFI_STA &&
      optionIndex == WendigoOptionSTASavedNetworks) {
    /* Update STA's saved networks */
    snprintf(optionValue, sizeof(optionValue), "%d Network%s",
      dev->radio.sta.saved_networks_count,
      (dev->radio.sta.saved_networks_count == 1) ? "" : "s");
  } else if (dev->scanType == SCAN_WIFI_AP &&
      optionIndex == WendigoOptionAPAuthMode) {
    /* Update AP authentication mode */
    uint8_t mode = dev->radio.ap.authmode;
    if (mode > WIFI_AUTH_MAX) {
      mode = WIFI_AUTH_MAX;
    }
    strncpy(optionValue, wifi_auth_mode_strings[mode], sizeof(optionValue));
  } else if (dev->scanType == SCAN_WIFI_STA &&
      optionIndex == WendigoOptionSTAAP) {
    /* Update STA's AP */
    /* ... But only if we have an actual MAC for the AP */
    if (memcmp(dev->radio.sta.apMac, nullMac, MAC_BYTES)) {
      /* AP has a MAC - Do we have the AP in our cache? */
      uint16_t apIdx = device_index_from_mac(dev->radio.sta.apMac);
      if (apIdx == devices_count || devices == NULL || devices[apIdx] == NULL
          || devices[apIdx]->scanType != SCAN_WIFI_AP ||
          devices[apIdx]->radio.ap.ssid[0] == '\0') {
        /* Either we don't have the AP in our cache or the
         * AP's SSID is unknown - Use MAC instead */
        bytes_to_string(dev->radio.sta.apMac, MAC_BYTES, optionValue);
      } else {
        /* We have an SSID for the AP */
        strncpy(optionValue, devices[apIdx]->radio.ap.ssid, sizeof(optionValue));
      }
    } else {
      /* We don't know the AP */
      snprintf(optionValue, sizeof(optionValue), "AP Unknown");
    }
  } else {
    /* Error state - Nothing to do */
  }

  /* Update menu and option labels */
  if (name != NULL && strlen(name) > 0) {
    variable_item_set_item_label(new_item, name);
  }
  if (free_name) {
    free(name);
  }
  variable_item_set_current_value_index(new_item, optionIndex);
  if (optionValue[0] != '\0') {
    variable_item_set_current_value_text(new_item, optionValue);
  }
  FURI_LOG_D(WENDIGO_TAG, "End wendigo_scene_device_list_update_device()");
}

/** Update the current display to reflect a new discovery result for `dev`.
 * This function is called by the functions wendigo_add_device() and
 * wendigo_update_device() in wendigo_scan.c. When this scene and scanning are
 * active at the same time all devices identified, whether newly-identified
 * devices or subsequent packets describing a known device, are passed as an
 * argument to this function to allow the UI to be dynamically updated and to
 * display either information about a new device or updated information about an
 * existing device.
 */
void wendigo_scene_device_list_update(WendigoApp *app, wendigo_device *dev) {
  FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_device_list_update()");
  /* This statement will also catch a NULL dev */
  if (!wendigo_device_is_displayed(dev)) {
    return;
  }
  /* Is this a new or existing device? */
  uint16_t dev_idx = custom_device_index(dev, current_devices.devices,
    current_devices.devices_count);
  if (dev_idx == current_devices.devices_count) {
    /* Add a new item */
    uint8_t options_count = wendigo_scene_device_list_options_count(dev);
    // TODO: Review parsers to see if there's any way dev could have an initialised view - Decide whether we need a free(dev->view). Only include if definitely necessary - it's risky, only required one initialisation mistake to cause crashes
    dev->view = variable_item_list_add(app->devices_var_item_list, "Loading",
      options_count, wendigo_scene_device_list_var_list_change_callback, dev);
    uint8_t option_idx = wendigo_scene_device_list_default_option(dev);
    /* Expand current_devices.devices[] & selected_option_index[] */
    wendigo_device **new_devices = realloc(current_devices.devices,
      sizeof(wendigo_device *) * (current_devices.devices_count + 1));
    uint8_t *new_selected_option_index = realloc(
      current_devices.selected_option_index, current_devices.devices_count + 1);
    if (new_devices == NULL || new_selected_option_index == NULL) {
      char *msg = malloc(sizeof(char) * 108);
      if (msg == NULL) {
        wendigo_log(MSG_ERROR, "wendigo_scene_device_list_update(): Failed to extend devices[] and selected_option_index[].");
      } else {
        snprintf(msg, 108,
          "wendigo_scene_device_list_update(): Failed to extend devices[] and selected_option_index[] to length %d.",
          current_devices.devices_count + 1);
        wendigo_log(MSG_ERROR, msg);
        free(msg);
      }
      /* If either realloc worked, shrink it back to its original size */
      if (new_devices != NULL) {
        current_devices.devices = realloc(new_devices,
          sizeof(wendigo_device *) * current_devices.devices_count);
      }
      if (new_selected_option_index != NULL) {
        current_devices.selected_option_index = realloc(
          new_selected_option_index, current_devices.devices_count);
      }
    } else {
      /* Append device and option in current_devices */
      current_devices.devices = new_devices;
      current_devices.devices[current_devices.devices_count] = dev;
      current_devices.selected_option_index = new_selected_option_index;
      current_devices.selected_option_index[current_devices.devices_count++] = option_idx;
    }
  }
  /* Set VariableItem label and option */
  wendigo_scene_device_list_update_device(dev->view);
  FURI_LOG_D(WENDIGO_TAG, "End wendigo_scene_device_list_update()");
}

/** Initialise selected_option_index for the specified device list using
 * default display options.
 * This will set the selected option index for all devices in deviceList
 * to device type (i.e. scanType).
 * Returns TRUE if the function completes successfully.
 */
bool wendigo_selected_options_init(DeviceListInstance *deviceList) {
  FURI_LOG_T(WENDIGO_TAG, "Start wendigo_selected_options_init()");
  if (deviceList == NULL || deviceList->devices_count == 0 ||
      deviceList->devices == NULL) {
    wendigo_log(MSG_ERROR, "End wendigo_selected_options_init() - Invalid arguments.");
    return false;
  }
  if (deviceList->selected_option_index == NULL) {
    deviceList->selected_option_index = malloc(deviceList->devices_count);
    if (deviceList->selected_option_index == NULL) {
      wendigo_log(MSG_ERROR, "End wendigo_selected_options_init() - Failed to allocate memory to hold device list selected options.");
      return false;
    }
    /* Loop through deviceList->devices[] to set appropriate option defaults.
     * Only do this if selected_option_index is NULL otherwise it'll be
     * impossible to scroll through options. */
    for (uint16_t i = 0; i < deviceList->devices_count; ++i) {
      deviceList->selected_option_index[i] =
        wendigo_scene_device_list_default_option(deviceList->devices[i]);
    }
  }
  return true;
}

/** Re-render the variable item list. This function exists because there is no
 * method to remove items from a variable_item_list, but that is sometimes
 * necessary (e.g. when viewing selected devices and de-selecting a device).
 */
void wendigo_scene_device_list_redraw(WendigoApp *app) {
  FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_device_list_redraw()");
  variable_item_list_reset(app->devices_var_item_list);
  uint8_t options_count = 0;
  wendigo_scene_device_list_set_current_devices_mask(current_devices.devices_mask);
  /* Initialise current_devices.selected_option_index[] if necessary */
  if (!wendigo_selected_options_init(&current_devices)) {
    wendigo_log(MSG_ERROR, "wendigo_selected_options_init() failed!");
  }
  /* Set header text for the list if specified. NULL it first to prevent text-over-text */
  variable_item_list_set_header(app->devices_var_item_list, NULL);
  if (current_devices.devices_msg[0] != '\0') {
    variable_item_list_set_header(app->devices_var_item_list,
      current_devices.devices_msg);
  }
  /* Add each device that's in scope */
  for (uint16_t i = 0; i < current_devices.devices_count; ++i) {
    /* Determine the number of options based on the device type */
    if (current_devices.devices != NULL &&current_devices.devices[i] != NULL) {
      options_count = wendigo_scene_device_list_options_count(current_devices.devices[i]);
      /* Add a new list item */
      current_devices.devices[i]->view = variable_item_list_add(
        app->devices_var_item_list, "Loading", options_count, 
        wendigo_scene_device_list_var_list_change_callback,
        current_devices.devices[i]);
      /* Set item's label and option */
      wendigo_scene_device_list_update_device(current_devices.devices[i]->view);
    }
  }
  /* Restore the selected item index from current_devices */
  variable_item_list_set_selected_item(app->devices_var_item_list, current_devices.selected_index);
  FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_device_list_redraw()");
}

static void wendigo_scene_device_list_var_list_enter_callback(void *context,
                                                              uint32_t index) {
  FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_device_list_var_list_enter_callback()");
  furi_assert(context);
  WendigoApp *app = context;

  furi_assert(index < current_devices.devices_count);

  /* Get the wendigo_device from the VariableItem at index `index` */
  wendigo_device *item = NULL;
  VariableItem *var_item = variable_item_list_get(
    app->devices_var_item_list, index);
  if (var_item != NULL) {
    item = (wendigo_device *)variable_item_get_context(var_item);
  }
  /* If that failed, currently we know that the var_item_list and
   * current_devices.devices[] are in the same order so we can look the device
   * up in current_devices.devices[].
   * NOTE: This will need to be removed when sorting is implemented. */
  if (var_item == NULL || item == NULL) {
    wendigo_device *item = current_devices.devices[index];
    if (item == NULL) {
      wendigo_log(MSG_ERROR, "End wendigo_scene_device_list_var_list_enter_callback(): Unable to find selected wendigo_device.");
      return;
    }
  }
  /* This is a good opportunity to patch item->view if it's invalid */
  item->view = var_item;

  current_devices.selected_index = index;

  /* Update the selected option */
  uint8_t option_index = variable_item_get_current_value_index(item->view);
  /* If the tag/untag menu item is selected toggle tagged state */
  if (item->view != NULL &&
      (((item->scanType == SCAN_HCI || item->scanType == SCAN_BLE) &&
        option_index == WendigoOptionBTTagUntag) ||
        (item->scanType == SCAN_WIFI_AP &&
        option_index == WendigoOptionAPTagUntag) ||
        (item->scanType == SCAN_WIFI_STA &&
        option_index == WendigoOptionSTATagUntag))) {
    item->tagged = !(item->tagged);
    variable_item_set_current_value_text(item->view,
                                        (item->tagged) ? "Untag" : "Tag");
    /* If the device is now untagged and we're viewing tagged devices only,
     * remove the device from view unless custom device view is enabled. */
    if (((current_devices.devices_mask & DEVICE_SELECTED_ONLY) == DEVICE_SELECTED_ONLY) &&
        !item->tagged && ((current_devices.devices_mask & DEVICE_CUSTOM) == 0)) {
      /* Bugger - There's no method to remove an item from a variable_item_list
       * We'll just have to redraw the entire var_item_list. */
      item->view = NULL;
      wendigo_scene_device_list_redraw(app);
    }
  } else if (item->view != NULL && ((item->scanType == SCAN_WIFI_AP &&
      option_index == WendigoOptionAPStaCount) ||
      (item->scanType == SCAN_WIFI_STA &&
      option_index == WendigoOptionSTAAP))) {
    /* Push current_devices onto the device list stack */
    DeviceListInstance *new_stack = realloc(stack, sizeof(DeviceListInstance) * (stack_counter + 1));
    if (new_stack == NULL) {
      wendigo_display_popup(app, "Insufficient Memory", "Unable to allocate an additional DeviceListInstance.");
      wendigo_log(MSG_ERROR, "wendigo_scene_device_list_var_list_enter_callback() terminated early. Unable to malloc() additional DeviceListInstance.");
      return;
    } else {
      memcpy(&(new_stack[stack_counter]), &current_devices, sizeof(DeviceListInstance));
      stack = new_stack;
      ++stack_counter;
    }
    /* Re-initialise current_devices */
    current_devices.devices_mask = DEVICE_CUSTOM;
    current_devices.view = WendigoAppViewDeviceList;
    current_devices.free_devices = true;
    current_devices.devices_count = 0;
    current_devices.devices = NULL;
    current_devices.selected_option_index = NULL;
    current_devices.selected_index = 0;
    bzero(current_devices.devices_msg, sizeof(current_devices.devices_msg));
    char *deviceName = malloc(sizeof(char) * (MAX_SSID_LEN + 1));
    if (deviceName == NULL) {
      /* Not using wendigo_log() so I can include %d */
      // TODO: Extend wendigo_log() to support variable arguments
      FURI_LOG_E("wendigo_scene_device_list_var_list_enter_callback()",
        "Failed to allocate deviceName[%d], proceeding without it.", MAX_SSID_LEN + 1);
    } else {
      bzero(deviceName, sizeof(char) * (MAX_SSID_LEN + 1));
    }
    if (item->scanType == SCAN_WIFI_AP) {
      current_devices.view = WendigoAppViewAPSTAs;
      if (item->radio.ap.stations_count > 0) {
        current_devices.devices = malloc(sizeof(wendigo_device *) * item->radio.ap.stations_count);
        current_devices.selected_option_index = malloc(item->radio.ap.stations_count);
      }
      if (item->radio.ap.stations_count > 0 && (current_devices.devices == NULL ||
          current_devices.selected_option_index == NULL)) {
        char *msg = malloc(sizeof(char) * 56);
        if (msg == NULL) {
          wendigo_log(MSG_ERROR,
            "Unable to allocate memory to display AP's stations.");
        } else {
          /* +1 below to account for selected_option_index */
          snprintf(msg, 56,
            "Unable to allocate %d bytes to display AP's stations.",
            (sizeof(wendigo_device *) + 1) * item->radio.ap.stations_count);
          wendigo_log(MSG_ERROR, msg);
          free(msg);
        }
        wendigo_display_popup(app, "Out of memory", "Unable to allocate memory for AP's stations.");
        current_devices.devices_count = 0;
        /* Check whether either were successfully allocated */
        if (current_devices.devices != NULL) {
          free(current_devices.devices);
          current_devices.devices = NULL;
        }
        if (current_devices.selected_option_index != NULL) {
          free(current_devices.selected_option_index);
          current_devices.selected_option_index = NULL;
        }
      } else { /* There are no stations to display or malloc() succeeded */
        current_devices.free_devices = true; /* Don't forget to only free if stations_count > 0 as well */
        /* Loop through item->radio.ap.stations, adding devices with the
         * specified MACs if we can find them. */
        uint16_t idx_src;
        uint16_t idx_dest;
        uint16_t idx_sta;
        for (idx_src = 0, idx_dest = 0;
            idx_src < item->radio.ap.stations_count; ++idx_src) {
          idx_sta = device_index_from_mac(item->radio.ap.stations[idx_src]);
          if (idx_sta < devices_count) {
            /* The station exists in the cache - add it to current_devices */
            current_devices.devices[idx_dest] = devices[idx_sta];
            /* We're displaying stations so default to RSSI */
            current_devices.selected_option_index[idx_dest++] = WendigoOptionSTARSSI;
          }
        }
        /* If there were stations not in the cache, current_devices will have empty
         * elements - if this occurs, shrink current_devices.devices[]. */
        if (idx_dest < item->radio.ap.stations_count) {
          wendigo_device **tmp_devices = realloc(current_devices.devices, sizeof(wendigo_device *) * idx_dest);
          if (tmp_devices != NULL) {
            /* If realloc() worked, just set devices[] to tmp_devices[] */
            current_devices.devices = tmp_devices;
          }
          /* Also shrink selected_option_index[] */
          uint8_t *new_options = realloc(current_devices.selected_option_index, idx_dest);
          if (new_options != NULL) {
            current_devices.selected_option_index = new_options;
          }
        }
        /* Set devices_count whether or not we had to shrink devices[] */
        current_devices.devices_count = idx_dest;
      }
      /* Set deviceName using SSID if we have it, otherwise MAC */
      if (deviceName == NULL) {
        snprintf(current_devices.devices_msg,
          sizeof(current_devices.devices_msg),
          "Stations");
      } else {
        if (item->radio.ap.ssid[0] == '\0') {
          bytes_to_string(item->mac, MAC_BYTES, deviceName);
        } else {
          strncpy(deviceName, item->radio.ap.ssid, sizeof(char) * (MAX_SSID_LEN + 1));
        }
        snprintf(current_devices.devices_msg,
          sizeof(current_devices.devices_msg),
          "%s STAs", deviceName);
      }
    } else if (item->scanType == SCAN_WIFI_STA) {
      current_devices.view = WendigoAppViewSTAAP;
      /* Use MAC to refer to the station */
      if (deviceName == NULL) {
        snprintf(current_devices.devices_msg,
          sizeof(current_devices.devices_msg),
          "Access Point");
      } else {
        bytes_to_string(item->mac, MAC_BYTES, deviceName);
        snprintf(current_devices.devices_msg,
          sizeof(current_devices.devices_msg),
          "%s' AP", deviceName);
      }
      /* Station will display one device if it has an AP, otherwise 0 */
      if (memcmp(item->radio.sta.apMac, nullMac, MAC_BYTES)) {
        /* We have a MAC. Find the wendigo_device* */
        uint16_t apIdx = device_index_from_mac(item->radio.sta.apMac);
        if (apIdx < devices_count) {
          /* Found the AP in the device cache - Display it */
          current_devices.devices_count = 1;
          current_devices.devices = &(devices[apIdx]);
          current_devices.free_devices = false;
          current_devices.selected_option_index = malloc(1);
          if (current_devices.selected_option_index == NULL) {
            wendigo_log(MSG_ERROR, "Unable to allocate 1 byte for AP's options.");
          } else {
            /* Default to displaying RSSI */
            current_devices.selected_option_index[0] = WendigoOptionAPRSSI;
          }
        }
      }
    } else {
      wendigo_log(MSG_WARN,
        "Logic error: Fell through conditional nest in wendigo_scene_device_list_var_list_enter_callback().");
    }
    if (deviceName != NULL) {
      free(deviceName);
    }
    view_dispatcher_send_custom_event(app->view_dispatcher,
      Wendigo_EventListDevices);
  } else if (item->view != NULL && item->scanType == SCAN_WIFI_STA &&
      option_index == WendigoOptionSTASavedNetworks) {
    /* Tell the scene which device we're interested in */
    wendigo_scene_pnl_list_set_device(item, app);
    view_dispatcher_send_custom_event(app->view_dispatcher,
      Wendigo_EventListNetworks);
  } else {
    /* Display details for `item` */
    wendigo_scene_device_detail_set_device(item);
    // TODO Fix detail scene, then
    // view_dispatcher_send_custom_event(app->view_dispatcher,
    // Wendigo_EventListDeviceDetails);
  }
  FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_device_list_var_list_enter_callback()");
}

/** Called when the selected option is changed for a device by scrolling
 * through the available options.
 */
static void wendigo_scene_device_list_var_list_change_callback(VariableItem *item) {
  FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_device_list_var_list_change_callback()");
  furi_assert(item);
  wendigo_device *dev = (wendigo_device *)variable_item_get_context(item);
  if (dev == NULL) {
    wendigo_log(MSG_WARN, "wendigo_scene_device_list_var_list_change_callback(): Context is NULL, unexpected behaviour may occur.");
  }
  /* Initialise selected_option_index[] if necessary */
  if (current_devices.selected_option_index == NULL) {
    bool result = wendigo_selected_options_init(&current_devices);
    if (!result) {
      wendigo_log(MSG_ERROR, "wendigo_scene_device_list_var_list_change_callback(): Failed to initialise selected_option_index().");
    }
  }
  /* Find the index of dev in current_devices.devices[] */
  uint16_t dev_idx = custom_device_index(dev, current_devices.devices,
    current_devices.devices_count);
  /* Update selected_option_index[] */
  if (current_devices.selected_option_index != NULL &&
      dev_idx < current_devices.devices_count) {
    uint8_t idx = variable_item_get_current_value_index(item);
    /* If idx is out of bounds use the default index for the device type */
    uint8_t options_count = wendigo_scene_device_list_options_count(dev);
    if (idx >= options_count) {
      idx = wendigo_scene_device_list_default_option(dev);
      /* Just to be extra-safe */
      if (idx >= options_count) {
        idx = 0;
      }
    }
    current_devices.selected_option_index[dev_idx] = idx;
  }
  /* Update UI attributes */
  wendigo_scene_device_list_update_device(item);
  FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_device_list_var_list_change_callback()");
}

/** Initialise the device list
 *  TODO: Consider sorting options when implemented
 */
void wendigo_scene_device_list_on_enter(void *context) {
  FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_device_list_on_enter()");
  FURI_LOG_T(WENDIGO_TAG, "current_view: %d, devices_count: %d, devices_mask: %d, devices_msg: %s",
    current_devices.view, current_devices.devices_count, current_devices.devices_mask,
    current_devices.devices_msg);
  WendigoApp *app = context;
  app->current_view = current_devices.view;

  wendigo_start_device_timer(app, deviceTimer, DEVICE_REFRESH_MS);

  /* Reset and re-populate the list */
  wendigo_scene_device_list_redraw(app);

  variable_item_list_set_enter_callback(app->devices_var_item_list,
    wendigo_scene_device_list_var_list_enter_callback, app);

  /* Ignore the scene state and restore the selected device from
   * current_devices. */
  uint8_t selected_item = current_devices.selected_index;
  if (selected_item >= current_devices.devices_count) {
    selected_item = 0;
  }
  variable_item_list_set_selected_item(app->devices_var_item_list, selected_item);
  view_dispatcher_switch_to_view(app->view_dispatcher, WendigoAppViewDeviceList);
  FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_device_list_on_enter()");
}

bool wendigo_scene_device_list_on_event(void *context,
                                        SceneManagerEvent event) {
//  FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_device_list_on_event()");
  WendigoApp *app = context;
  bool consumed = false;

  if (event.type == SceneManagerEventTypeCustom) {
    switch (event.event) {
    case Wendigo_EventListDeviceDetails:
      scene_manager_next_scene(app->scene_manager, WendigoSceneDeviceDetail);
      break;
    case Wendigo_EventListDevices:
      /* current_devices has been populated with relevant devices - all we
       * need to do here is display them. */
      scene_manager_next_scene(app->scene_manager, WendigoSceneDeviceList);
      break;
    case Wendigo_EventListNetworks:
        scene_manager_next_scene(app->scene_manager, WendigoScenePNLList);
        break;
    default:
      char *msg = malloc(sizeof(char) * 54);
      if (msg != NULL) {
        snprintf(msg, 54,
                "wendigo_scene_device_list received unknown event %ld.",
                event.event);
        wendigo_log(MSG_WARN, msg);
        free(msg);
      }
      break;
    }
    consumed = true;
  } else if (event.type == SceneManagerEventTypeTick) {
    current_devices.selected_index =
        variable_item_list_get_selected_item_index(app->devices_var_item_list);
    consumed = true;
  }
//  FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_device_list_on_event()");
  return consumed;
}

void wendigo_scene_device_list_on_exit(void *context) {
  FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_device_list_on_exit()");
  WendigoApp *app = context;
  variable_item_list_reset(app->devices_var_item_list);
  for (uint16_t i = 0; i < devices_count; ++i) {
    devices[i]->view = NULL;
  }
  if (app->leaving_scene) {
    /* This condition is met when we are genuinely exiting this scene - when
     * the back button has been pressed. When displaying a device list from
     * another device list, such as displaying an AP's STAs, this function is
     * called but we do not want to replace the current_devices we've just
     * constructed with the stack element we've just pushed. */

    /* Stop & free the refresh timer if it's running */
    if (deviceTimer != NULL) {
      furi_timer_stop(deviceTimer);
      furi_timer_free(deviceTimer);
      deviceTimer = NULL;
    }
    /* Free current_devices.devices[] if necessary */
    if (current_devices.devices != NULL && current_devices.devices_count > 0) {
      if (current_devices.free_devices) {
        free(current_devices.devices);
      }
      if (current_devices.selected_option_index != NULL) {
        free(current_devices.selected_option_index);
      }
      current_devices.devices = NULL;
      current_devices.selected_option_index = NULL;
      current_devices.devices_count = 0;
    }
    /* Pop the previous device list off the stack if there's one there */
    if (stack_counter > 0) {
      /* Copy the DeviceListInstance, otherwise it'll be freed during use */
      memcpy(&current_devices, &(stack[stack_counter - 1]), sizeof(DeviceListInstance));
      /* When we pop the final stack element realloc() will act like free()
       * and return NULL */
      DeviceListInstance *stackAfterPop = realloc(stack, sizeof(DeviceListInstance) * (stack_counter - 1));
      if (stackAfterPop == NULL && stack_counter > 1) {
        wendigo_log(MSG_ERROR,
          "Unable to shrink DeviceListInstance stack. Hoping for the best...");
      } else {
        stack = stackAfterPop;
      }
      --stack_counter;
    }
    app->leaving_scene = false;
  }
  FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_device_list_on_exit()");
}
