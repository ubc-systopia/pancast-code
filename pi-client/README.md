# PanCast Raspberry Pi Serial Transfer

The Client app is to be run on a Raspberry Pi. The client will fetch risk broadcast data periodically from the backend and send the data to the Silicon Labs 
Bluetooth application for broadcast over BLE.

## Getting Started

### Setting-up a new Raspberry-Pi

1. Use Raspberry Pi Imager to load OS onto the SD card, further instructions [here](https://projects.raspberrypi.org/en/projects/raspberry-pi-setting-up/2)
2. Connect Raspberry Pi to wifi and enable SSH, further instructions [here](https://phoenixnap.com/kb/enable-ssh-raspberry-pi) for enabling SSH. These will both require a monitor, mouse and keyboard the first time until we can find a headless mode workaround.
3. SSH into raspberry pi from PC on the same wifi network.

    `ssh pi@raspberrypi.local`

4. Once you have SSH connection with raspberry pi, clone client code from repo

    `git clone -b rpi https://github.com/ubc-systopia/pancast-code.git`

5. Install required libraries on Raspberry Pi:

    `sudo apt-get install libcurl4-openssl-dev`

6. Export GPIO pin 24 , instructions [here](https://www.ics.com/blog/gpio-programming-using-sysfs-interface).

## Compiling the Application

1. Navigate to `src` directory and use `make` to compile the app.

## Running the Application

1. Make sure the Silicon Labs Start kit is flashed with the beacon app and that the app is set to run in network mode. Instructions for set-up [here](https://github.com/ubc-systopia/pancast-code/tree/main/beacon).
2. Connect the GPIO pins on the raspberry pi to the EXT header pins on the Silicon Labs board using jumper wires. Connect ground to ground and PB00 on 
    Silicon Labs EXT header to GPIO24 on the Raspberry Pi. Raspberry Pi pin labels can be found [here](https://www.raspberrypi-spy.co.uk/2012/06/simple-guide-to-the-rpi-gpio-header-and-pins/). Images of set-up below: \
   <img src="https://user-images.githubusercontent.com/32030240/159351813-c54f52a1-b4a5-4c91-b6b1-9dd57715b8f9.jpeg" width="300">
   <img src="https://user-images.githubusercontent.com/32030240/159351766-522f88a4-a709-4952-a3d1-1417e81a4d34.jpeg" width="300">

3. Make sure that the broadcast device (Silicon Labs board) is connected to the Raspberry Pi via USB and that both devices are on. 
4. Run the application using `sudo ./client` from the `src` directory where the app has been built.
