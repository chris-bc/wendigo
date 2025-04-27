# [NOT YET WORKING] esp32-wendigo
ESP32 firmware to accompany the Wendigo Flipper Zero application.

ESP32-Wendigo **will be** an ESP32 firmware to monitor and analyse WiFi, Bluetooth Classic and Bluetooth Low Energy devices. See https://github.com/chris-bc/wendigo for more information.

Unlike previous applications, both my own and the typical approach used for Flipper Zero applications that use an external microcontroller, ESP32-Wendigo **will not** provide a CLI/console application suitable for standalone use; many Flipper Zero/ESP applications use the Flipper Zero solely as the user interface, meaning that the ESP application can be run just as effectively (sometimes moreso, given the increased screen real estate) from a serial console application such as Minicom. The bulk of Wendigo is implemented in the Flipper Zero application, with a lightweight protocol used to communicate commands and data between the ESP32 and Flipper Zero, and the ESP32 (more or less) continually transmitting data to the Flipper Zero.

Support for devices other than Flipper Zero can be provided by creating an application that implements Wendigo's protocol to send appropriate commands to ESP32-Wendigo and interpret and display the data sent by it. Details of the protocol are provided below.


## Building & Flashing


## Compatibility

### Memory Requirements
ESP32-Wendigo requires a moderate amount of IRAM, with wireless stacks and Bluetooth attribute dictionaries being particularly memory-hungry. ESP-IDF (Espressif's development framework) has been configured to reduce IRAM use by disabling unused components and/or having components use flash storage rather than IRAM where this will not affect performance.

If you encounter memory errors while compiling ESP32-Wendigo, which may happen as a result of reconfiguring the application to enable additional components (including logging) or by using an ESP32 module with particularly small memory capacity, the following configuration changes can be applied to make more IRAM available to ESP32-Wendigo. Each of these changes is likely to reduce either the performance or compatibility of the application so they are recommended only if needed.

The recommended way to make these changes is by using ``` idf.py menuconfig ``` as described in the section *Building & Flashing*, although the file ```sdkconfig``` may be updated directly if this is not possible.
* Disable WiFi options ```CONFIG_ESP_WIFI_IRAM_OPT``` and/or ```CONFIG_ESP_WIFI_RX_IRAM_OPT```
  * I don't believe these options will be used based on Wendigo's design, which will save memory without consequence. If I'm wrong this will reduce the performance of processing WiFi options
* Set ```CONFIG_ESP32_REV_MIN``` to 3 to disable PSRAM bug workarounds and increase available IRAM
  * If your ESP32 does not have PSRAM bugs this will reduce memory use without consequence;
  * If your ESP32 has a PSRAM bug requiring workaround the workaround will be run from flash rather than IRAM, reducing performance
* Disable ```CONFIG_ESP_EVENT_POST_FROM_IRAM_ISR``` to allow ```esp_event``` to post events without generating IRAM-safe interrupt handlers
  * I'm not very clear on this setting and its impact. ESP32-Wendigo doesn't directly use ```esp_event``` although it's possible the WiFi, Bluetooth, and/or BLE stacks use it for event processing
  * Espressif's documentation doesn't describe negative consequences from disabling it so I suspect performance degradation is the only negative consequence
* Disable all unnecessary manufacturers from the configuration section **Auto-detect Flash Chips** to remove support for unused hardware
  * ```Component Config``` -> ```SPI Flash Driver``` -> ```Auto-Detect Flash Chips```
  * A multitude of ESP32 devices are available from a wide array of manufacturers, each of whom may use a different flash storage chip on their device. You only need ESP32-Wendigo to work with the flash chip on your specific device so, if you know how to determine the manufacturer of the flash chip on your ESP32, you can disable the other manufacturers to improve both IRAM use and performance.

You can find more information about memory usage [from Espressif's developer documentation](https://docs.espressif.com/projects/esp-idf/en/v5.4.1/esp32/api-guides/performance/ram-usage.html#optimizing-iram-usage).

## Protocol
