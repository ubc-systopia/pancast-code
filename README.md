# PanCast Code
Code for some components of the PanCast system, including an implementation of the beacon to dongle advertisement protocol for encounter tracing.

## Features
### Critical Functionality

Working features include:

- BLE broadcast/receive
- Ephemeral ID generation
- Encounter logging
- Device configuration load from flash
- OTP storage
- Terminal connection - delayed release upload
- Log deletion (e.g. past 14 Days)

Problems and missing features are documented on the [issues](https://github.com/ubc-systopia/pancast-code/issues) page.

## Structure
Application code is found in the various directories of the project.

* `beacon` -- SiLabs beacon with Gecko SDK (network beacon)
* `dongle` -- SiLabs dongle with Gecko SDK
* `zephyr-beacon` -- Nordic nRF52832 beacon with Zephyr OS (BLE-only beacon)
* `common` -- common header files
* `test-apps` -- sample Bluetooth applications used for testing and benchmarking
* `pi-client` -- raspberry pi client used to download risk data from the backend and forward to the network beacon over a serial connection. For details see the [Raspberry Pi Set-up](https://docs.google.com/document/d/1yTDDE8dWmT4W_3zhqdPPBc3t0FCl_lEvqI6VNVbjvZs/edit?usp=sharing).


The code is located in the `common`, `beacon`, `zephyr-beacon`, and `dongle` , and `terminal` directories, under `src`. The terminal application is not a full implementation of the PanCast terminal but is rather a demo/testing tool.

## Setup

The Pancast project currently uses Silicon Labs Gecko SDK version **3.2.0**

### Gecko (Silicon Labs) Platform

* Install Simplicity Studio 5 from https://www.silabs.com/developers/simplicity-studio. This application is used for development and flashing the boards.
* Note: If using MacOS Big Sur, a [workaround](https://silabs-prod.adobecqms.net/content/usergenerated/asi/cloud/content/siliconlabs/en/community/software/simplicity-studio/forum/jcr:content/content/primary/qna/mac_os_can_t_importdemocodeformsimplicitystdi-ej4M.social.$%7BstartIndex%7D.15.html) is needed to begin development with Simplicity Studio.
Below is a snippet of the workaround steps:

```
sudo /usr/local/bin/python3 -m pip install jinja2 pyxb html2text
cd "/Applications/Simplicity Studio.app/Contents/Eclipse/developer/adapter_packs/python/bin/"
bin % mv python python.orig
mv python3 python3.orig
mv python3.6 python3.6.orig
ln -s /usr/local/bin/python3 python
ln -s /usr/local/bin/python3
ln -s /usr/local/bin/python3.6
```

### Zephyr Platform

* Follow the instructions to get Zephyr and install Python dependencies from [here](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/zephyr/getting_started/index.html#getting-started).
* For MacOS, following is a snippet of the steps:

```
brew tap ArmMbed/homebrew-formulae
brew install arm-none-eabi-gcc
brew install python3
brew install cmake ninja gperf python3 ccache qemu dtc
pip3 install -U west
mkdir ncs; cd ncs
west init -m https://github.com/nrfconnect/sdk-nrf --mr v1.7.1
west update
west zephyr-export
sudo pip3 install -r zephyr/scripts/requirements.txt
sudo pip3 install -r nrf/scripts/requirements.txt
sudo pip3 install -r bootloader/mcuboot/scripts/requirements.txt
```

## Application development and usage

Follow the application-specific steps in:

* [`beacon/README.md`](beacon/README.md)
* [`dongle/README.md`](dongle/README.md)
* [`zephyr-beacon/README.md`](zephyr-beacon/README.md)
