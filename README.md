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
    Wendigo is a Flipper Zero application to detect and follow WiFi, Bluetooth and BLE radio signals, allowing them to be monitored and tracked to their location. Wendigo is a creature from North American mythology that stalks and overpowers its victims.
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

Wendigo is a Flipper Zero application to detect, monitor and track nearby wireless devices. It uses detected WiFi, Bluetooth Classic, and Bluetooth Low Energy signals to fingerprint a device, monitoring identified device(s) to inspect their behaviour and track their location.

<p align="right">(<a href="#readme-top">back to top</a>)</p>


<a id="getting-started"></a>
## Getting Started

Once the application has a functional base binaries will be made available under *Releases*. Alternatively, especially if you want the latest features or to contribute to the development, you can build the application from source, 

<a id="prerequisites"></a>
### Prerequisites

<a id="flipper-prerequisites"></a>
#### Flipper Zero
Download source code for your preferred firmware. The most popular unofficial distributions are Momentum, RogueMaster and Unleashed, although a variety of other distributions are also available.

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
Download ESP-IDF, the Espressif IoT Development Framework. If you use VSCode you can download the extension *ESP-IDF by Espressif Systems* from the VSCode Extension Marketplace and follow the setup wizard. This guide focuses on building and flashing using the command line so if you opt for this approach you're on your own! :)

  ```sh
  git clone https://github.com/espressif/esp-idf.git
  ```

<a id="wendigo-prerequisites"></a>
#### Wendigo
  ```sh
  git clone https://github.com/chris-bc/wendigo.git
  ```
  To check out a specific tagged version:
  ```sh
  git checkout <tag name>
  ```

<a id="installation"></a>
### Installation

These instructions are written under the assumption that the above packages have been downloaded to the following locations. Substitute those locations for your own in the instructions that follow.
* Flipper Zero firmware: ~/flipperzero
* ESP-IDF: ~/esp-idf
* Wendigo: ~/wendigo

1. Configure and Install ESP-IDF
   ```sh
   cd ~/esp-idf
   ./install.sh
   ```
2. Configure your current terminal session for ESP-IDF. This must be run prior to a new terminal session running idf.py.
   ```sh
   . ./export.sh
   ```
3. Connect an ESP32 and verify it has been detected by the OS
   ```sh
   ls /dev/tty*
   ```
4. Compile and flash ESP32-Wendigo
   ```sh
   cd ~/wendigo/esp32
   idf.py set-target <target chipset>
   idf.py menuconfig
   idf.py build flash
   ```
5. Install the Flipper Zero toolchain and compile firmware
   ```sh
   cd ~/flipper
   ./fbt
   ```
6. Remove the ESP32, connect a Flipper Zero, and verify it has been detected by the OS
   ```sh
   ls /dev/tty*
   ```
7. Link Flipper-Wendigo into the Flipper Zero firmware, build and flash the firmware
   ```sh
   ln -s ~/wendigo/flipper ~/flipper/applications_user/wendigo
   ./fbt firmware_all flash_usb_full
   ```

<p align="right">(<a href="#readme-top">back to top</a>)</p>



<a id="usage"></a>
## Usage

Connect the ESP32 to the Flipper Zero by connecting the following pins:
* Flipper Zero TX (13) to ESP32 RX
* Flipper Zero RX (14) to ESP32 TX
* Flipper Zero GND (8 or 18) to ESP32 GND
* Flipper Zero 3V3 (9) to ESP32 3V3 (sometimes labelled VCC)

Launch the Wendigo application from Apps -> GPIO -> ESP -> [ESP32] Wendigo.

_For more examples, please refer to the [Documentation](https://example.com)_

<p align="right">(<a href="#readme-top">back to top</a>)</p>



<a id="roadmap"></a>
## Roadmap

- [ ] Feature 1
- [ ] Feature 2
- [ ] Feature 3
    - [ ] Nested Feature

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

### Top contributors:

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
