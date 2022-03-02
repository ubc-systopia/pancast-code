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

1. Connect the GPIO pins on the raspberry pi to the EXT header pins on the Silicon Labs board using jumper wires. Connect ground to ground and PB00 on 
    Silicon Labs EXT header to GPIO24 on the Raspberry Pi. 
2. Make sure that the broadcast device (Silicon Labs board) is connected to the Raspberry Pi via USB and that both devices are on. 
3. Run the application using `sudo ./client` from the `src` directory where the app has been built.
