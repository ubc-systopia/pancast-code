# Raspberry Pi Client App

A client app to be run on a Raspberry Pi. Recieves data from the Pancast server and sends this data over the serial port to the bluetooth beacon.

## Getting Started

1. Format the SD card. Plug SD card into PC and load Raspbian OS if necessary. 
    - This can be done using the Raspberry Pi Imager app (https://www.raspberrypi.org/software/). Select `Raspberry Pi OS 32 bit` and default storage.
    - Then copy setup script (pancast-code/risk_client/pi-setup/setup.sh) to /boot/
2. Once the SD card is formatted, insert into the raspberry pi and plug in the power cable.
3. To set-up wifi for now: you will need a desktop monitor, mouse and keyboard plugged into the Raspberry Pi. Power on RPI and select UBC Visitor from 
  the wifi drop down on the main desktop. Open a browser and accept the UBC Visitor terms and conditions.
4. Once wifi is connected, run the setup script on the rpi command line:
    `sudo /boot/setup.sh`
5. Add the boot script to `/etc/rc.local`
6. Then copy risk_client code from the pancast-code repository to the raspberry pi. 

## Compiling the Application

## Running the Application
