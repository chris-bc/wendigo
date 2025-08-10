<a id="readme-top"></a>

<!-- PROJECT SHIELDS -->
[![Contributors][contributors-shield]][contributors-url]
[![Forks][forks-shield]][forks-url]
[![Stargazers][stars-shield]][stars-url]
[![Issues][issues-shield]][issues-url]
[![MIT License][license-shield]][license-url]
[![LinkedIn][linkedin-shield]][linkedin-url]

<!-- PROJECT LOGO -->
<br />
<div align="center">
  <a href="https://github.com/chris-bc/wendigo">
    <img src="The_Wendigo.jpg" alt="Logo" width="80" height="80">
  </a>

<h3 align="center">Wendigo</h3>

  <p align="center">
    Wendigo is a creature from North American mythology that stalks and overpowers its victims. If The Wendigo has you in its sights there's no escaping. You won't hear it coming and what happens next is up to The Wendigo.
    <br />
    <br />
    <a href="https://github.com/chris-bc/wendigo/issues/new?labels=bug&template=bug-report---.md">Report Bug</a>
    &middot;
    <a href="https://github.com/chris-bc/wendigo/issues/new?labels=enhancement&template=feature-request---.md">Request Feature</a>
  </p>
</div>

<!-- TABLE OF CONTENTS -->
<details>
  <summary>Table of Contents</summary>
  <ol>
    <li>
      <a href="#about-the-project">About The Project</a>
    </li>
    <li><a href="#whats-new">What's New</a>
      <ul>
        <li><a href="#whats-new-040">v0.4.0</a></li>
        <li><a href="#whats-new-030">v0.3.0</a></li>
      </ul>
    </li>
    <li>
      <a href="#getting-started">Getting Started</a>
      <ul>
        <li><a href="#prerequisites">Prerequisites</a></li>
        <ul>
          <li><a href="#flipper-prerequisites">Flipper Zero</a></li>
          <li><a href="#esp32-prerequisites">ESP32</a></li>
          <li><a href="#wendigo-prerequisites">Wendigo</a></li>
        </ul>
        <li><a href="#installation">Installation</a></li>
      </ul>
    </li>
    <li><a href="#usage">Usage</a></li>
    <li><a href="#roadmap">Roadmap</a></li>
    <li><a href="#contributing">Contributing</a></li>
    <li><a href="#license">License</a></li>
    <li><a href="#contact">Contact</a></li>
  </ol>
</details>

<a id="about-the-project"></a>

## About The Project

Wendigo is an application for the Flipper Zero and ESP32 microcontrollers. It is primarily a WiFi and Bluetooth scanner/sniffer, with a few extra tricks up its sleeve. Wendigo is in its early stages of development, with features and bugfixes regularly committed, and releases to mark significant milestones.

Why write Wendigo when there are already great apps like [Marauder](https://github.com/justcallmekoko/ESP32Marauder) and [GhostESP](https://github.com/jaylikesbunda/Ghost_ESP) that do this and more? Flipper Zero has a large and fairly complete SDK (Software Development Kit) that makes it easy to create interactive interfaces; all of the Flipper + microcontroller apps I've seen display information in a serial console and require you to select devices and enter commands in a console, using the fairly clunky Flipper Zero keyboard. Wendigo has a graphical interface - instead of scrolling through pages of text to determine you want to select Access Point #13, and then typing ```13``` with the keyboard, in Wendigo you use the navigation buttons to navigate to the Access Point you're interested in, then scroll through the available commands and select the one you want.

My hope is that Wendigo will be a quick and easy way to get information about a wireless device; make it easier to troubleshoot and diagnose issues; and provide an interesting and accessible platform to explore the wireless spectrum and understand the data that it exposes. I think of Marauder as the tool you use to carry out an attack, and Wendigo as the tool to help you find the right target.

At a high level, these are Wendigo's features - Both implemented and planned:

* [X] Bluetooth Classic discovery
* [X] Bluetooth Low Energy discovery
* [X] WiFi Access Point discovery (2.4GHz)
* [X] WiFi Station (client device) discovery (2.4GHz)
* [X] WiFi channel hopping (2.4GHz)
* [X] Select which WiFi channels and which radios are used for scanning
* [X] Console Mode to use ESP32-Wendigo with other tools (e.g. laptop)
* [X] Tagging (selecting) devices of interest to view only them
* [X] Display discovered devices in a native Flipper Zero menu
  * [X] Device type
  * [X] Bluetooth device name (if present) or Bluetooth Device Address (BDA - looks like a MAC)
  * [X] Access Point SSID (if present) or MAC
  * [X] RSSI
  * [X] Bluetooth Class of Device (CoD)
  * [X] WiFi channel
  * [X] Access Point authentication mode (WEP, WPA2, etc.)
  * [X] SSID that a WiFi Station (client) is connected to
  * [X] Time since device was last seen
  * [X] Tag/Untag to select devices of interest
  * [X] List dynamically updates as new devices, or additional information about existing devices, are found
* [X] Select which device types are displayed (BT Classic, BLE, WiFi Access Points, WiFi Stations, All Bluetooth, All WiFi, All Devices)
* [ ] Display detailed device information
* [ ] Bluetooth service discovery
* [ ] Bluetooth attribute read/write
* [ ] Browse Bluetooth devices and their services & attributes
* [X] Browse WiFi networks, navigating from an Access Point to its connected Stations or from a Station to its Access Point
* [ ] Deauthenticate a device or all devices on a network
* [X] Collect & display a Station's (WiFi client - such as a phone) saved network list
* Put this list into Wigle to get a good idea where the device's owner lives, works and plays
* [X] Display all networks that probes were received for and support browsing from probed SSIDs to the Stations that probed for them.
* [ ] 5GHz WiFi channels ([requires ESP32-C5](https://www.aliexpress.com/item/1005009128201189.html))
* [ ] Change Bluetooth BDA and WiFi MAC
* [ ] Use Flipper Zero's LED to indicate events

More might come after this - Or it might not. My previous Flipper app, [Gravity](https://github.com/chris-bc/flipper-gravity), tried to include everything including the kitchen sink, and as a result it did everything poorly. My primary goal with Wendigo is a quick and easy way to do reconnaissance and extract as much information as possible from a device(s), displaying everything using native Flipper UI components.

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<a id="whats-new"></a>

## What's New

<a id ="whats-new-050"></a>

### v0.5.0 (So far)

* Because crowded areas get poor results running BLE and WiFi simultaneously, restructured menu to simplify running one or the other.
* (TODO) Additional packets: associate, authenticate
* (TODO) BT services

<a id="whats-new-040"></a>

### v0.4.0

* Introduced mutexes to manage concurrency during packet transimission and reception
  * Concurrency issues - multiple threads sending or receiving packets at once - were the reason for packet corruption and all the headaches I've had trying to work around it.
  * New mutex to guarantee only a single ESP32-Wendigo thread can transmit a packet at a time
  * New mutex to guarantee that a single thread will work on Flipper-Wendigo's UART buffer at a time
  * I'd like to manage concurrency of Flipper-Wendigo's device cache as well, but want to implement a read-write lock for that, and that'll take a little time
  * This change has eliminated practically all instability of both apps - I've still made mistakes, but you have to look harder to find them!
* A WiFi station's Preferred Network List (PNL), commonly called "saved networks", are collected and displayed
  * Device list display for stations includes a new menu option ```N networks``` (where N is a number).
  * Select this to view a list of the SSIDs the station has sent probe requests for.
* New menu option to view all probed networks and the number of probed SSIDs
  * Selecting this option will display all probed SSIDs and the number of devices searching for each SSID.
    * From this view, selecting an SSID will display all stations that probed for that SSID.
    * This required introducing a second data model. It gets a mutex too!
* Navigate from a WiFi Station to its AP by selecting the AP menu option and pushing ```OK```.
  * This is a new Device List display, but displaying only the AP
  * You can select and view attributes in the same way as the device list
    * You can even view the AP's stations, select the station you were previously looking at, view its AP, view the AP's stations, ...
  * Use the ```back``` button to restore the previous view
* Navigate from a WiFi Access Point to its Stations by selecting the menu option labelled ```N stations```.
  * A new Device List will be displayed containing the Stations of the selected AP
  * As above, this is a fully-functional device list.

That just about wraps it up for 2.4GHz WiFi! The AP authentication mode is a joke and needs to be fixed, and there are some tweaks and quality-of-life improvements I'd like to make, but I should start thinking about the next feature to tackle. If anyone is reading this and wants to make a request I'm all ears - Bluetooth service discovery; sending beacon and deauth packets; displaying status information with the LED; 5GHz WiFi; MAC spoofing; detailed device view; sorting the device list; ...

<a id="whats-new-030"></a>

### v0.3.0

* Work around possible bug in FZ API. See [source code](https://github.com/chris-bc/wendigo/blob/main/Flipper/scenes/wendigo_scene_device_list.c) for details
* 2.4GHz WiFi scanning/sniffing - Decodes eight 802.11 packet types
* Bluetooth Classic Discovery (requires devices to be in pairing mode)
* Bluetooth Low Energy discovery
* Device list dynamically updates during scanning
* Device List displays
  * WiFi Access Points
    * SSID if available, otherwise MAC
    * RSSI
    * Tag / Untag
    * Device Type ("WiFi AP")
    * Number of connected stations discovered
    * Authentication Mode (Open, WPA, etc.)
      * Currently buggy.
    * Wireless channel
    * Time since last seen
  * WiFi Stations
    * MAC
    * RSSI
    * Tag / Untag
    * Device Type ("WiFi STA")
    * Connected Access Point
      * SSID if available, otherwise MAC
    * Wireless channel
    * Time since last seen
  * Bluetooth Classic & Low Energy
    * Device name if available, otherwise BDA (equivalent to MAC)
    * RSSI
    * Tag / Untag
    * Device Type ("BT Classic" or "BLE")
    * Bluetooth Class of Device ("Phone", "PC", "Wearable", etc.)
    * Time since last seen

If the 2.4GHz spectrum is busy I've found that the radio only supplies BLE
packets - In my home I need to disable BLE in the ```Setup``` menu in order
to receive WiFi packets. Sorry about that - I don't think there's anything I
can do, other than change the UI so it's no longer possible to run both WiFi
and Bluetooth scanning concurrently. And I hate the idea of removing a
feature.

<a id="getting-started"></a>

## Getting Started

Wendigo is still under active development. As you can see from the above list there is still a lot to do before a version 1.0 release. Currently it can scan for and display Bluetooth (Classic and Low Energy) and WiFi (2.4GHz) devices; collate and display devices' saved networks; and not much else. Because it's under active development you might find a bug. If the Main branch is ever unuseable you'll have better luck with a release, which you can download from the [Releases](https://github.com/chris-bc/wendigo/releases) page or check out using the release tags.

Binaries for Flipper Zero and ESP32 are available from the [Releases](https://github.com/chris-bc/wendigo/releases) page. Significant releases will provide binaries for a variety of ESP32 models but - for now at least - minor releases include just standard ESP32 (Wroom) binaries. If you use a different ESP32 model, or want the latest features or to contribute to development, you can compile the application yourself as described below. These instructions assume you're using a Unix-like OS - MacOS, Linux or BSD. Windows users can either install WSL or adapt the instructions to suit windows.

<a id="prerequisites"></a>

### Prerequisites

<a id="flipper-prerequisites"></a>

To use Wendigo you'll need a Flipper Zero and an ESP32 microcontroller. Any model of ESP32 should work fine - Wendigo probes for available features and will disable Bluetooth and/or 5GHz WiFi if they're not supported by the chip. I'd recommend against using an ESP32-S2 because it lacks Bluetooth support. ESP32-C5 is currently the only model with 5GHz WiFi support, which will be added to Wendigo within the next few months. You can pick one up for under AUD$25 from [Espressif's official Ali Express store](https://www.aliexpress.com/item/1005009128201189.html).

#### Flipper Zero

Download source code for your preferred Flipper Zero firmware. The most popular unofficial distributions are [Momentum](https://github.com/next-flip/Momentum-Firmware), [RogueMaster](https://github.com/RogueMaster/flipperzero-firmware-wPlugins) and [Unleashed](https://github.com/DarkFlippers/unleashed-firmware), although a variety of other distributions are also available.

* Momentum Firmware:

  ```sh
  git clone https://github.com/Next-Flip/Momentum-Firmware.git
  ```

* RogueMaster Firmware:

  ```sh
  git clone https://github.com/RogueMaster/flipperzero-firmware-wPlugins.git
  ```

* Unleashed Firmware:

  ```sh
  git clone https://github.com/DarkFlippers/unleashed-firmware.git
  ```

* Official Firmware:

  ```sh
  git clone https://github.com/flipperdevices/flipperzero-firmware.git
  ```

<a id="esp32-prerequisites"></a>

#### ESP32

Download ESP-IDF, the Espressif IoT Development Framework. If you use VSCode you can download the extension *ESP-IDF by Espressif Systems* from the VSCode Extension Marketplace and follow the setup wizard. This guide focuses on building and flashing using the command line, but the setup wizard for the VSCode ESP-IDF extension makes it a breeze to get up and running.

  ```sh
  git clone https://github.com/espressif/esp-idf.git
  ```

<a id="wendigo-prerequisites"></a>

#### Wendigo

  ```sh
  git clone https://github.com/chris-bc/wendigo.git
  ```

  To check out a specific tagged version use ```git checkout <tag name>```, for example:

  ```sh
  git checkout tags/v0.1.0
  ```

<a id="installation"></a>

### Building & Flashing

These instructions are written under the assumption that the above repositories have been downloaded to the following locations. Substitute those locations for your own in the instructions that follow.

* Flipper Zero firmware: ~/flipperzero
* ESP-IDF: ~/esp-idf
* Wendigo: ~/wendigo

1. Configure and Install ESP-IDF

   ```sh
   cd ~/esp-idf
   ./install.sh
   ```

2. Configure your current terminal session for ESP-IDF. This must be done before running ```idf.py``` in your current terminal session (note that if you have multiple terminal windows open you must run this in *every* terminal window you would like to run ```idf.py``` in).

   ```sh
   source ./export.sh
   ```

3. Connect an ESP32 and verify it has been detected by the OS.

   ```sh
   ls /dev/tty*
   ```

   It should appear (on Linux) as ```ttyUSB0``` or ```ttyACM0```.

4. Compile and flash ESP32-Wendigo

   ```sh
   cd ~/wendigo/esp32
   idf.py set-target <target chipset>
   (optional) idf.py menuconfig
   idf.py build flash
   ```

   ```<target chipset>``` is the model of ESP32 you are using. For example ```esp32```, ```esp32-s3```, etc.

   You only need to run ```menuconfig``` if there's a configuration setting you'd like to change. The repository default is configured for a standard ESP32 Wroom.

5. Install the Flipper Zero toolchain and compile firmware

   ```sh
   cd ~/flipperzero
   ./fbt
   ```

6. Configure the Flipper Build Tool (FBT) for your current terminal session. As with step (2), this needs to be done in each terminal session you wish to use FBT.

   ```sh
   source `./fbt -s env`
   ```

7. Remove the ESP32, connect a Flipper Zero, and again verify that it has been detected by the OS

   ```sh
   ls /dev/tty*
   ```

8. Link Flipper-Wendigo into the Flipper Zero firmware, build and flash the firmware

   ```sh
   ln -s ~/wendigo/flipper ~/flipperzero/applications_user/wendigo
   ./fbt firmware_all flash_usb_full
   ```

9. Alternatively, to flash just Wendigo and not the entire firmware:

   ```sh
   ./fbt launch APPSRC=wendigo
   ```

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<a id="usage"></a>

## Usage

Connect the ESP32 to the Flipper Zero by connecting the following pins:

* Flipper Zero TX (13) to ESP32 RX
* Flipper Zero RX (14) to ESP32 TX
* Flipper Zero GND (8 or 18) to ESP32 GND
* Flipper Zero 3V3 (9) to ESP32 3V3 (sometimes labelled VCC)

Launch the Wendigo application from Apps => GPIO => ESP => [ESP32] Wendigo BT+BLE+WiFi Monitor. The ```Setup``` menu allows you to select which radios are enabled (Bluetooth Classic, Bluetooth Low Energy, WiFi) and which WiFi channels are included in channel hopping, otherwise you can just start scanning and check out the results.

Alternatively you can connect ESP32-Wendigo to any device with a serial console and control it from there. Connect your serial console in the usual way, for example ```screen /dev/ttyUSB0 115200```, then enter the command ```i 1``` (or ```interactive 1```) to switch from Binary Mode to Interactive Mode. You now have tab completion, command history, and can use the commands ```commands``` to get a list of available commands and ```help``` to get a brief overview and syntax of each command.

*For more information about Interactive Mode please refer to the [Command Documentation](https://example.com)*

### TODO: Screenshots

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<a id="roadmap"></a>

## Roadmap

This section is a running list of current priorities.

* [ ] ESP32 tag command has a radio arg, doesn't need it - parse_command_tag()
* [X] Scan menu option doesn't need "Start" when it's started or "Stop" when it's stopped - Use a single menu option that changes its text, similar to Tag/Untag.
* [X] Combined Bluetooth packet for BT Classic and LE devices
* [X] Combined Bluetooth data model for Flipper
* [X] BT Classic and LE device transmission from ESP32 to Flipper
* [X] BT Classic and LE packet parsing and caching on Flipper Zero
* [X] Enable/Disable radios
* [X] Scan Start/Stop
* [X] Scanning status
  * [X] Supported radios
  * [X] Status of radios
  * [X] Devices found
* [X] ESP Version (popup)
* [X] Display devices
  * [X] Display device name instead of BDA when present
  * [X] Dynamically update device list on new/updated devices
  * [X] Show attributes in device list (options menu)
    * [X] RSSI
    * [X] Device type
    * [X] CoD
    * [X] Tag/Untag (status and command in one)
    * [X] Age since last seen - Implemented but nonsense results.
  * [X] Device tagging and displaying only tagged devices
* [ ] WiFi Features
  * [X] WiFi packet
  * [X] ESP32 WiFi scanning
  * [X] ESP32 WiFi packet transmission
  * [X] Flipper WiFi packet parsing
  * [X] Flipper WiFi data model
    * [X] Move data model definitions into a common header file, included by both ESP32 and Flipper apps
    * [X] Standardise data model as a struct with common attributes, and a union of specialised structs.
  * [X] Channel hopping
  * [X] Capture a STA's Preferred Network List
    * [X] Extract PNL SSID's from probe requests
    * [X] Maintain PNL in data model
    * [X] Transmit PNL in packet to Flipper-Wendigo
    * [X] Parse PNL out of packet in Flipper-Wendigo
    * [X] Reconstruct PNL in Flipper-Wendigo's data model
    * [X] Display PNL in device list
      * [X] New STA option "%d SSIDs"
      * [X] New var_item_list-based scene wendigo_scene_network_list
      * [X] Displays new scene
    * [X] Display preferred networks from all devices in a single view
      * [X] Display the number of devices probing for each SSID
      * [X] Display all devices that have probed for a particular SSID
  * [ ] Support for 5GHz channels (ESP32-C5)
  * [X] Display different options in wendigo_scene_device_list
    * [X] For STA
      * [X] RSSI
      * [X] Tag/Untag
      * [X] ScanType
      * [X] AP (SSID if present, otherwise MAC)
      * [X] Channel
      * [X] LastSeen
    * [X] For AP
      * [X] RSSI
      * [X] Tag/Untag
      * [X] ScanType
        * [X] AuthMode
      * [X] Channel
      * [X] LastSeen
* [ ] Bluetooth Services
  * [ ] ESP32 service discovery
  * [ ] ESP32 service transmission
  * [ ] Flipper service parsing
  * [ ] Flipper service data model
* [ ] Settings
  * [ ] Retrieve and change MACs
  * [X] Enable/Disable channels
* [ ] Display device details
  * [ ] Update details when device updated
  * [ ] Use canvas view so properties can be layed out to (hopefully) make everything visible without scrolling
* [ ] "Display Settings" menu
  * [X] Enable/Disable device types
  * [ ] Sort results by
    * [ ] Name
    * [ ] LastSeen
    * [ ] RSSI
    * [ ] Device Type
* [ ] Other Features
  * [ ] Focus Mode
  * [ ] Use FZ LED to indicate events
  * [ ] variable_item_list (Flipper API) uses uint8_t to get/set selected item index, meaning the device list is limited to 255 devices.
    * [ ] Add some fanciness to allow display of a larger number of devices.
  * [ ] Ability to flash ESP32 firmware from Flipper?
* [ ] Flipper Zero logging
  * [ ] Develop a Flipper-based logging mechanism and support for multiple streams of data from ESP32
  * [ ] Logs which go directly to a buffer or file;
  * [ ] Device info which goes directly to display
  * [ ] Refactor all logging, error handling and status functionality
    * [ ] In FZ mode dispatch to FZ logging mechanism
* [ ] Improve FZ memory management if needed
  * [ ] Continuous scanning no longer seems to exhaust memory, but further testing is needed.
  * [ ] Hopefully find a FreeRTOS hook or config so I can provide a function when low on memory
  * [ ] Instead, prune device cache when low on memory
    * [ ] Configurable techniques as with Gravity - prune based on RSSI, time since last seen, or tagged status
    * [ ] Allow different techniques for different radios
  * [ ] If FreeRTOS or FZ hook can't be found, estimate an upper bound for device cache through trial & error

See the [open issues](https://github.com/chris-bc/wendigo/issues) for a full list of proposed features (and known issues).

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<a id="contributing"></a>

## Contributing

Contributions are what make the open source community such an amazing place to learn, inspire, and create. Any contributions you make are **greatly appreciated**.

If you have a suggestion that would make this better, please fork the repo and create a pull request. You can also simply open an issue with the tag "enhancement".
Don't forget to give the project a star! Thanks again!

1. Fork the Project
2. Create your Feature Branch (`git checkout -b feature/AmazingFeature`)
3. Commit your Changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the Branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

<p align="right">(<a href="#readme-top">back to top</a>)</p>

### Top contributors

<a href="https://github.com/chris-bc/wendigo/graphs/contributors">
  <img src="https://contrib.rocks/image?repo=chris-bc/wendigo" alt="contrib.rocks image" />
</a>

<a id="license"></a>

## License

Distributed under the MIT Licence. See `LICENSE` for more information.

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<a id="contact"></a>

## Contact

Chris BC - [@chris_bc](https://twitter.com/chris_bc) - chris@bennettscash.id.au

Project Link: [https://github.com/chris-bc/wendigo](https://github.com/chris-bc/wendigo)

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- MARKDOWN LINKS & IMAGES -->
<!-- https://www.markdownguide.org/basic-syntax/#reference-style-links -->
[contributors-shield]: https://img.shields.io/github/contributors/chris-bc/wendigo.svg?style=for-the-badge
[contributors-url]: https://github.com/chris-bc/wendigo/graphs/contributors
[forks-shield]: https://img.shields.io/github/forks/chris-bc/wendigo.svg?style=for-the-badge
[forks-url]: https://github.com/chris-bc/wendigo/network/members
[stars-shield]: https://img.shields.io/github/stars/chris-bc/wendigo.svg?style=for-the-badge
[stars-url]: https://github.com/chris-bc/wendigo/stargazers
[issues-shield]: https://img.shields.io/github/issues/chris-bc/wendigo.svg?style=for-the-badge
[issues-url]: https://github.com/chris-bc/wendigo/issues
[license-shield]: https://img.shields.io/badge/License-MIT-yellow.svg?style=for-the-badge
[license-url]: https://opensource.org/licenses/MIT
[linkedin-shield]: https://img.shields.io/badge/-LinkedIn-black.svg?style=for-the-badge&logo=linkedin&colorB=555
[linkedin-url]: https://linkedin.com/in/chrisbennettscash
[product-screenshot]: images/screenshot.png
[Next.js]: https://img.shields.io/badge/next.js-000000?style=for-the-badge&logo=nextdotjs&logoColor=white
[Next-url]: https://nextjs.org/
[React.js]: https://img.shields.io/badge/React-20232A?style=for-the-badge&logo=react&logoColor=61DAFB
[React-url]: https://reactjs.org/
[Vue.js]: https://img.shields.io/badge/Vue.js-35495E?style=for-the-badge&logo=vuedotjs&logoColor=4FC08D
[Vue-url]: https://vuejs.org/
[Angular.io]: https://img.shields.io/badge/Angular-DD0031?style=for-the-badge&logo=angular&logoColor=white
[Angular-url]: https://angular.io/
[Svelte.dev]: https://img.shields.io/badge/Svelte-4A4A55?style=for-the-badge&logo=svelte&logoColor=FF3E00
[Svelte-url]: https://svelte.dev/
[Laravel.com]: https://img.shields.io/badge/Laravel-FF2D20?style=for-the-badge&logo=laravel&logoColor=white
[Laravel-url]: https://laravel.com
[Bootstrap.com]: https://img.shields.io/badge/Bootstrap-563D7C?style=for-the-badge&logo=bootstrap&logoColor=white
[Bootstrap-url]: https://getbootstrap.com
[JQuery.com]: https://img.shields.io/badge/jQuery-0769AD?style=for-the-badge&logo=jquery&logoColor=white
[JQuery-url]: https://jquery.com 
