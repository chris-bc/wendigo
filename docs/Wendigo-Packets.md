<a id="top"></a>

## Wendigo Protocol

Wendigo implements a binary protocol in order for ESP32-Wendigo to send device and other details to Flipper-Wendigo. In effect, ESP32-Wendigo acts as a wireless bridge, receiving data over wireless spectra and re-transmitting that using a protocol that Flipper-Wendigo can interpret. By implementing this protocol anyone can create a new Wendigo client receiving data from ESP32-Wendigo. This document describes the packets that may be sent by ESP32-Wendigo.

<a id="overview"></a>

### Overview

A Wendigo packet commences with a 4-byte "*preamble*" that defines the type of packet being transmitted. At the end of the packet is a 4-byte "*packet terminator*" marking the end of the packet. The following packet types are supported:

* <a href="#wifi-ap">WiFi Access Point</a>
* <a href="#wifi-sta">WiFi Station</a>
* <a href="#bluetooth">Bluetooth device (Bluetooth Classic or Low Energy)</a>
* <a href="#status">Status</a>
* <a href="#version">Version</a>
* <a href="#channels">Enabled WiFi Channels</a>
* <a href="#mac">MAC addresses (WiFi MAC and Bluetooth Device Address (BDA))</a>

<p align="right"><a href="#top">Back to top</a></p>

<a id="wifi-ap"></a>

### WiFi Access Point

An access point packet is composed of:

* Preamble: ```0x99, 0x98, 0x97, 0x96``` (4 bytes)
* Device type: ```0x02``` (1 byte ```uint8_t```)
* MAC address: 6 bytes
* Channel: 1 byte (```uint8_t```)
* RSSI: 2 bytes (```int16_t```)
* Time since last seen: 19 bytes (```struct timeval```)
* Tagged (selected) status: ```0```: device is not tagged, ```1```: device is tagged (1 byte ```uint8_t```)
* Authentication mode: 1 byte (```uint8_t```)
* SSID length: 1 byte (```uint8_t```)
* Station count (the number of discovered WiFi stations that are connected to this AP): 1 byte (```uint8_t```)
* SSID: Length defined by **SSID Legnth** above
* Connected stations: **Station count** MAC addresses, with each MAC being 6 bytes
* Packet terminator: ```0xAA, 0xBB, 0xCC, 0xDD``` (4 bytes)

<p align="right"><a href="#top">Back to top</a></p>

<a id="wifi-sta"></a>

### WiFi Station

A **station**, or WiFi client device, is represented by the following packet:

* Preamble: ```0x88, 0x87, 0x86, 0x85``` (4 bytes)
* Device type: ```0x03``` (1 byte ```uint8_t```)
* MAC address: 6 bytes
* Channel: 1 byte (```uint8_t```)
* RSSI: 2 bytes (```int16_t```)
* Time since last seen: 19 bytes (```struct timeval```)
* Tagged (selected) status: ```0```: device is not tagged, ```1```: device is tagged (1 byte ```uint8_t```)
* Saved networks count: 1 byte (```uint8_t```)
* Access point's MAC address: 6 bytes
  * If the station's access point has not been discovered this is 6 NULL bytes (```0x00```)
* Access point's SSID length: 1 byte (```uint8_t```)
  * If the station's access point has not been discovered this is ```0x00```
* Access point's SSID: Length defined by **SSID Length** above
* Saved networks: **Saved networks count** repetitions of:
  * SSID length (of the saved network): 1 byte (```uint8_t```)
  * SSID (of the saved network): length defined above
* Packet terminator: ```0xAA, 0xBB, 0xCC, 0xDD``` (4 bytes)

<p align="right"><a href="#top">Back to top</a></p>

<a id="bluetooth"></a>

### Bluetooth Device

Bluetooth Classic and Bluetooth Low Energy devices are both represented by this packet type.

* Preamble: ```0xFF, 0xFE, 0xFD, 0xFC``` (4 bytes)
* Device name length: 1 byte (```uint8_t```)
* EIR length: 1 byte (```uint8_t```)
* RSSI: 2 bytes (```int16_t```)
* Class of device: 4 bytes (```uint32_t```)
* Bluetooth Device Address (Bluetooth equivalent of MAC): 6 bytes
* Device type: Bluetooth Classic: ```0x00```, Bluetooth Low Energy: ```0x01``` (1 byte ```uint8_t```)
* Tagged (selected) status: ```0```: device is not tagged, ```1```: device is tagged (1 byte ```uint8_t```)
* Time since last seen: 19 bytes (```struct timeval```)
* Number of advertised Bluetooth services: 1 byte (```uint8_t```)
* Length of known Bluetooth services: 1 byte (```uint8_t```)
* Length of short class of device description (e.g. *Misc*, *Phone*, etc.): 1 byte (```uint8_t```)
* Bluetooth device name: **Device name length** bytes
* EIR: **EIR legnth** bytes
* Short class of device description: **Length of short class of device** bytes
* Packet terminator: ```0xAA, 0xBB, 0xCC, 0xDD``` (4 bytes)

<p align="right"><a href="#top">Back to top</a></p>

<a id="status"></a>

### Status

A status packet provides a summary of discovered device counts by device types as well as information about capabilities supported by ESP32-Wendigo. For instance, if an ESP32 running Wendigo does not support Bluetooth, such as an ```ESP32-S2```, Bluetooth features in Wendigo will be disabled. This packet type is how this information is communicated to Flipper-Wendigo.

* Preamble: ```0x66, 0x65, 0x64, 0x63``` (4 bytes)
* Binary representation of supported features (1 byte)
  * This is the result of logical-OR-ing SupportedHardwareMask values
* Attribute count: Number of attributes included in the packet: 1 byte (```uint8_t```)
* **Attribute count** repetitions of:
  * Attribute name length: 1 byte (```uint8_t```)
  * Attribute name: **Attribute name length** bytes
  * Attribute value length: 1 byte (```uint8_t```)
  * Attribute value: ***Attribute value length** bytes
* Packet terminator: ```0xAA, 0xBB, 0xCC, 0xDD```

<p align="right"><a href="#top">Back to top</a></p>

<a id="version"></a>

### Version

The version packet is very short and simple. As a simple packet it provides a useful test case when investigating communication issues.

* Preamble: "```Wend```" (4 bytes)
* Remainder of version string: "```igo vx.y.z```" (11 byte ```char[]```, including terminating NULL (```0x00```))
* Packet terminator: ```0xAA, 0xBB, 0xCC, 0xDD``` (4 bytes)

<p align="right"><a href="#top">Back to top</a></p>

<a id="channels"></a>

### WiFi Channels

The channels packet informs Flipper-Wendigo which channels are enabled, so this information can be displayed in the UI and changed by the user.

* Preamble: ```0x77, 0x76, 0x75, 0x74``` (4 bytes)
* Channel count: The number of enabled channels (1 byte ```uint8_t```)
* **Channel count** ```uint8_t``` bytes, each representing an enabled channel
* Packet terminator: ```0xAA, 0xBB, 0xCC, 0xDD```

<p align="right"><a href="#top">Back to top</a></p>

<a id="mac"></a>

### MAC addresses

This packet provides Flipper-Wendigo with the ESP32's WiFi MAC address and Bluetooth Device Address.

* Preamble: ```0x55, 0x54, 0x53, 0x52``` (4 bytes)
* Count of interfaces in the packet (1 byte ```uint8_t```)
* **Count of interfaces** repetitions of:
  * Interface type: ```0x00```: Base MAC, ```0x01```: WiFi MAC, ```0x02```: Bluetooth MAC (1 byte ```uint8_t```)
  * MAC (6 bytes)
* Packet terminator: ```0xAA, 0xBB, 0xCC, 0xDD``` (4 bytes)

<p align="right"><a href="#top">Back to top</a></p>