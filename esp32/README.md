# [WORK IN PROGRESS] esp32-wendigo
ESP32 firmware to accompany the Wendigo Flipper Zero application.

ESP32-Wendigo is an ESP32 firmware to monitor and analyse WiFi, Bluetooth Classic and Bluetooth Low Energy devices. See https://github.com/chris-bc/wendigo for more information.

Many Flipper Zero applications that interact with an external microcontroller provide a lightweight facade to a serial console. These microcontroller applications can typically be run no less effectively, and sometimes with a great deal more useability due to the increased screen size, by connecting to the microcontroller from a computer using a serial console application such as **Minicom**. This is **NOT** the case with Wendigo. ESP32-Wendigo is intended to act as a bridge between the wireless world and the physical, providing a continual stream of data to Flipper Zero that represents what is currently happening on the WiFi and Bluetooth radio spectra.

An important design decision of Wendigo is to create it as a first-class Flipper Zero application, taking advantage of the excellent development framework created by the Flipper team to provide an interactive and easy-to-navigate interface to the wireless world. Because of this a great deal of what makes Wendigo *Wendigo* is stored on the Flipper Zero; while any serial console can communicate with ESP32-Wendigo, ESP32-Wendigo alone does not, and cannot, provide the features Wendigo aims to deliver.

*However*, care has been taken during development to support running ESP32-Wendigo as a standalone application where it makes sense. There are even some commands that are **not** availalble on Flipper Zero (such as ```status```) provided to support running ESP32-Wendigo standalone. Wendigo's scanning features can readily be used from any serial console to discover and interrogate nearby devices, and an **Interactive Mode** has been included to support this use case. **Interactive Mode** provides readable responses to commands, uses device caching to reduce the volume of output, and takes other steps to improve the utility of ESP32-Wendigo when run standalone.

**Interactive Mode** is controlled using the command ```i[nteractive]```. Its arguments are ```0 (disable)```, ```1 (enable)```, and ```2 (status)```. For example:

```sh
interactive 1
```

Or, equivalently:

```sh
i 1
```

Support for devices other than Flipper Zero can be provided by creating an application that implements Wendigo's protocol to send appropriate commands to ESP32-Wendigo and interpret and display the data sent by it. Details of the protocol are provided below.


## Building & Flashing


## Compatibility

### Memory Requirements
ESP32-Wendigo requires a moderate amount of IRAM, with wireless stacks and Bluetooth attribute dictionaries being particularly memory-hungry. ESP-IDF (Espressif's development framework) has been configured to reduce IRAM use by disabling unused components and/or having components use flash storage rather than IRAM where this will not affect performance.

If you encounter memory errors while compiling ESP32-Wendigo, which may happen as a result of reconfiguring the application to enable additional ESP32 features or by using an ESP32 module with particularly small memory capacity, the following configuration changes can be applied to make more IRAM available to ESP32-Wendigo. Each of these changes is likely to reduce either the performance or compatibility of the application so they are recommended only if needed.

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

### ESP32-Wendigo Command Sequencing

When ESP32-Wendigo has completed its startup tasks it will send the string ```\nREADY\n``` (where "\n" denotes a carriage return). Wendigo is ready to accept the commands described below. Due to the sometimes-flaky nature of UART communication, ESP32-Wendigo acknowledges receipt of all commands. If a client does not receive confirmation the command should be re-transmitted until it is acknowledged.

Some example dialogues that demonstrate the communication pattern are:

```sh
esp32> v
v ACK
Wendigo v0.1.0
v OK
```

```sh
esp32> h 1
h 1 ACK
I (423305) BTDM_INIT: BT controller compile version [42e1f02]
I (423315) BTDM_INIT: Bluetooth MAC: d4:8a:fc:xx:xx:xx
I (423315) phy_init: phy_version 4860,6b7a6e5,Feb  6 2025,14:47:07
I (424035) HCI@Wendigo: event: 10
h 1 OK
HCI 3c:22:fb:xx:xx:xx RSSI -65
esp32> h 0
h 0 ACK
h 0 OK
```

```sh
esp32> h 3
h 3 INVALID
h 3 FAIL
```

### ESP32-Wendigo Commands

The following commands are available from ESP32-Wendigo. All commands can be run using either their first letter or their full name.

#### hci

Controls the status of Bluetooth Classic Scanning.

#### ble

Controls the status of Bluetooth Low Energy Scanning.

#### wifi

Controls the status of WiFi scanning

#### ver

Displays the running version of ESP32-Wendigo.

#### interactive

Controls ESP32-Wendigo's Interactive Mode. When enabled ESP32-Wendigo will provide enhanced output intended for a human reader, rather than the terse and machine-oriented output otherwise displayed.

#### status

When in **Interactive Mode** this displays a summary of key ESP32-Wendigo information including communication protocols supported by the running ESP32 chipset, and operations currently in progress.

#### tag

**Not yet implemented**

Tags a device of interest. When viewing scan results tagged devices are highlighted to make them readily-identifiable; when **Follow Mode** is enabled only tagged devices will be displayed.

#### follow

**Not yet implemented**

Controls ESP32-Wendigo's Follow Mode. In Follow Mode ESP32-Wendigo only displays information about devices that have been **tagged**, and restructures the interface from chronological ("which devices have I seen most recently?") to spatial ("how strong is the signal from these devices?"). This makes it possible to locate the source of wireless signals that are of interest.

### Command Arguments

The hci, ble, wifi, follow, and interactive commands follow the same syntax, requiring a single argument to specify whether to enable, disable, or query the status of the feature.

  * 0: Disable
  * 1: Enable
  * 2: Status

The tag command also follows this pattern, although requires an additional argument to specify the device of interest. Its syntax is ```tag <addr> [ 0 | 1 | 2 ]```.

The first argument is the device address. This is the MAC address for WiFi devices, and the BDA for Bluetooth devices (it looks the same as a MAC address), provided in standard hex notation (for example 20:E8:82:EE:D7:D4).

The second, optional, argument is the action to take. If this is not specified the default is to tag the specified device. If it is already tagged the command will have no effect. Following the convention described above, the argument 0 will disable a tag, 1 will enable a tag, and 2 will report the tagged status of the specified device.

### Scan Results

When in Interactive Mode, device information is displayed to the console as devices are discovered. Outside of Interactive Mode a binary protocol is used to transmit this information from ESP32-Wendigo to Flipper-Wendigo. This format provides some basic error detection alongside a simple serialisation of the data structure used to represent devices. These messages can be reconstructed using the following method.