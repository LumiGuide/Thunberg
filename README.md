# THUNBERG CLIMATE CONTROL

An infrared controller for Fuji and Mitsubishi climate control units.
Visit thunberg.lumi.guide and select the room of the climate control unit you want to control under 'Room:'. It will load the last settings that were sent to this unit using the website. Set the preferred temperature, mode, strenth and extra options under 'toppings'. Click 'Send' to send a signal with these settings to the climate control unit, or click 'Turn off' to turn the unit off.
At the bottom of the page, you can click 'KILL ALL' to turn off all units in the rooms listed under 'Room:'.
Every day between 19:00 and 19:15, all units will be turned off automatically.


## Troubleshooting
- No response from the climate control unit
The Thunberg device works with an IR-LED, make sure there is nothing blocking the IR signal from the Thunberg device to the climate control unit. Make sure the IR-LED is pointing directly at the climate control unit.

If the LED on the back of the device is green, the device is connected to the internet. If this is not the case, unplug the device and plug it in again to reboot. It should automatically connect to the internet.
If the wifi name and/or password has changed, you need to change this in the menuconfig and flash the device. (Open the menuconfig by navigating to the Thunberg folder and using the command `idf.py menuconfig` in your terminal. ESP-IDF should be installed and running for this.)

- The webpage keeps loading
If there's no room selected, the page will remain on the loading screen. As soon as you select a room, it will load the last settings that were sent to the climate control unit in this room.
If there's a room selected, but the page keeps showing the loading screen, the website doesn't get a response from the device you selected. Check the internet connection of the device.

- The webpage is offline
The Master Device is offline. Unplug the device and plug it in again to reboot. It should automatically reconnect to the internet.

- It's still not working
Connect the device to your computer using the USB programmer and plug in the charger. With ESP-IDF running, type `idf.py -p USB-PORT monitor` in your terminal to see the data log.


# HOW TO BUILD

## Parts
- M5Stack M5Stamp Pico, ESP32-PICO-D4 (https://www.tinytronics.nl/shop/en/platforms-and-systems/m5stack/m5stamp/m5stamp/m5stack-m5stamp-pico-mate-esp32-pico-d4)
- IR LED, super-bright, 5mm, 940nm (https://www.adafruit.com/product/387)
- USB connector, USB4110-GF-A (https://www.digikey.nl/nl/products/detail/gct/USB4110-GF-A/10384547)
- Mosfet, N-channel, BSS214NWH6327XTSA1 (https://www.digikey.nl/nl/products/detail/infineon-technologies/BSS214NWH6327XTSA1/5959965https://www.digikey.nl/nl/products/detail/infineon-technologies/BSS214NWH6327XTSA1/5959965)
- Resistor, 100 Ohm, RC0603FR-07100RL (https://www.digikey.nl/nl/products/detail/yageo/RC0603FR-07100RL/726888)
- Resistor, 33.2 Ohm, CRCW060333R2FKEAHP (https://www.digikey.nl/nl/products/detail/vishay-dale/CRCW060333R2FKEAHP/2222239)
- PCB from file `thunberg.kicad_pcb` and `thunberg.kicad_sch`
- Case, 3D-printed from files `thunberg_case_body.stl` and `thunberg_case_lid.stl`
- M2 x 12mm screw, countersunk head (https://www.kingmicroschroeven.nl/bzk-inbus-verzonkenkopschroef-rvs-din-7991-m2x12.html)
- USB-C charger
- USB programmer FT232RL (https://www.otronic.nl/ft232rl-usb-ttl-serial-port-adapter-33v-5v.html)
- Breadboard and jumper wires to connect the USB programmer to the M5Stamp Pico
- ESP-IDF


## Soldering
Shorten the legs of the IR LED to about 4mm.
Solder the components on the PCB, using the `thunberg.kicad_sch` file for component names and `thunberg.kicad_pcb` for placement details.
Take the plastic casing of the M5Stamp Pico and solder the PCB on top of the M5Stamp Pico.


## Flash the device
Enter the WiFi settings in the menuconfig by navigating to the Thunberg folder and using the command `idf.py menuconfig` in your terminal (ESP-IDF should be installed and running for this) and go to `Thunberg Climate Control`.
Set the WiFi name under `WiFi SSID` and the corresponding password under `WiFi password`.

You'll need as much static IP-addresses as devices.
In `spiffs_data/index.html`, change the IP-addresses and corresponding room names if needed.

Connect the M5Stamp Pico to your laptop using the USB programmer and plug in the charger.
Flash one device using the host-settings. You can open the menuconfig by using the command `idf.py menuconfig` in your terminal (ESP-IDF should be installed and running for this). Go to `Thunberg Climate Control` > `Host website on this device` and select `Yes, host website`. This one will be the Master Device.
Select the right climate control brand under `Select airco brand`.
Save and close the menuconfig and flash the device by using the command `idf.py -p USB-PORT flash` in your terminal.

Flash the other devices without hosting a website on them. You can change this setting by opening the menuconfig by using the command `idf.py menuconfig` in your terminal and change the setting `Host website on this device` to `No, receive signals only` and make sure to select the right climate control brand.

Run the program on every single device while monitoring it using `idf.py -p USB-PORT monitor`.
In the terminal, you will be able to fetch its MAC-address in the line: `wifi:mode : sta (MAC)`.
In your router settings, link the MAC addresses of all devices to a static IP-address, and match these with the IP-addresses used in `spiffs_data/index.html`.
Optional: Create a convenient URL for the IP-address of the Master Device.


## How to use
Place the PCBs inside the cases and secure the lid with a screw. Mount the devices on the wall with the IR LED facing the climate control unit. The casing has some room for the IR LED to be adjusted. Power the devices with an USB-C charger.
When the LED on the back of the device turns green, it is connected to the internet.
Visit the IP-address of the Master Device. Here you can select the room of the climate control unit you want to control and the settings. 'Send' will send a signal to the selected climate control unit, ' Turn off' will turn this unit off. 'Kill all' will turn of all units in the rooms that are listed in the 'Room:' list on top of the page.
At the bottom of the page you see the date & time of the GET request and the last NTP synchronisation. This data can be used to see if the time and date are up to date. This is important for the next feature:
From monday to Friday, between 19:00 and 19:15, all units will be turned off automatically by sending a turn-off signal to every device.


## OTA
`/ota/new`:
To update the software, host a server with the compiled software in a `.bin`  file.
Place a POST request to the `/ota/new` URI of the IP-address of the device you want to update with the URL to the `.bin` file in the body. (You can use a program like Postman for this request.) The device will download the file and update the software. It will then restart and run the updated software. The old software is stored in another OTA slot in the Flash memory.

`/ota/commit`:
After running and testing the new software, you either have to commit this version or go back to the old version. Without committing, the version will remain in status ESP_OTA_IMG_PENDING_VERIFY and it will not be possible to update the software to a newer version. Also, in case of a restart, it will automatically change the status of this version to ESP_OTA_IMG_ABORTED and therefor it will not run.
To commit, place a POST request to the `/ota/commit` URI of the IP-address of the device you are updating.

`/ota/rollback`:
If you wish to go back to the old software version, it is possible to change the status of this version to ESP_OTA_IMG_ABORTED and restart the device. This version will not run due to its status and the old version will be selected with the reboot.
To roll back to the old version, place a POST request to the `/ota/rollback` URI of the device you were updating.


## TO DO
It's not possible to update the HTML-file using OTA updates. A software update should be written to provide this function.