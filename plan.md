# Initial Prototype

### Menu
* Setup
    * Enable BLE
    * Enable BD
    * Enable WiFi
    * Channels
        * All
        * Selected
            * 1: On, Off
            * 2: On, Off
            * 3: etc.
    * MAC Address
* Scan
    * Start
    * Stop
    * Status
* Devices
    * List of discovered devices
        * Name
        * RSSI
        * Protocol
        * Last seen
        * UUID / Services
        * Pair/Bond
        * Selected?
* Selected Devices
    * List of selected devices
        * As above
* Track selected devices
    * RSSI metrics
* About

### Next steps
* Retain selected channels when "all" is selected
  * Remove processing of enter when "all" is selected - just set a flag on app
* BT, BLE and WiFi on/off selections mapped to app variables
* Get/Set MAC leave until ESP32 app further along
* Start scan -> disable setup
    * Settings should still be available to view, but read-only
    * Use an app->settings_locked flag
* Stop scan -> enable setup
* Is there a way to prevent byte_input (MAC) from being editable? Or not display the keyboard?

#### THEN commence work on esp32-wendigo
* Perform sufficient initialisation of all radios to be able to get/set MACs
* Base on console application again
* Mock up a protocol to accept commands from FZ and output device info at random intervals

#### Push application logic to Flipper this time
* Activate UART interface on launch
* Start scan passes the configured settings to ESP32 and processes responses indefinitely
* Data from ESP32 processed into an object model to represent devices
* Scan status summarises settings

#### Iterate
* Replace mocks with real device data
* Refine data model and develop device list and detail UIs based on available data