# PanCast Code
Code for some components of the PanCast system, including an implementation of the beacon to dongle advertisement protocol for encounter tracing.

## Features
### Critical Functionality
| Name | Completion | Notes |
|------|------------|-------|
| Bluetooth LE Broadcast/Recieve | 90 % | Still working on modifying API to use 31 bytes for payload|
| Ephemeral ID Generation | 100 % ||
|Encounter Logging| 100 % |  |
| Device Configuration Load from Flash | 100 % | |
| OTP Storage | 100 % |  |
| Terminal Connection - Delayed Release Upload | 100 % ||

## Structure
Application code is found in the various directories of the project.

### Zephyr OS Applications (For Nordic Boards)

There are dongle, beacon, and terminal implementations written for Zephyr OS (plan to port these soon/built in mulit-SDK support, including for Silicon Labs devices). The code is located in the `common`, `beacon`, and `dongle` , and `terminal` directories, under `src`. 

The latter three are the applications and rely on the Zephyr project stack. See the following section for details. The terminal application is not a full implementation of the PanCast terminal but is rather a demo/testing tool.

### Network Beacon

Silicon Labs Set-up: https://docs.google.com/document/d/1BJARla0MJZo6spp_89Fs_hWgHd7yTF0c25zpfyrGhVA/edit?usp=sharing

Raspberry Pi Set-up: https://docs.google.com/document/d/1yTDDE8dWmT4W_3zhqdPPBc3t0FCl_lEvqI6VNVbjvZs/edit?usp=sharing

### Risk Client

Silicon Labs Set-up: https://docs.google.com/document/d/1BJARla0MJZo6spp_89Fs_hWgHd7yTF0c25zpfyrGhVA/edit?usp=sharing

## Development for Zephyr Applications
### General Setup
1. Make sure you have the Zephyr project cloned to a location on the development machine, and have followed the setup documentation [here](https://docs.zephyrproject.org/latest/getting_started/index.html) (in particular, you should have the `west` command available).

### Building the Apps
1. Navigate to the root Zephyr directory (the one containing the samples directory)
2. Issue the following command: `west build -p auto -b nrf52dk_nrf52832 <app_path> -- -Wno-dev -DCMAKE_EXPORT_COMPILE_COMMANDS=ON` where `<app_path>` is the full path to the application to be built (For example: '$HOME/projects/pancast-code/ble-encounters/beacon').

### Flashing the App
1. Make sure the development board is plugged in.
2. In the root Zephyr directory, run:   `west flash`, to flash the currently built application.
3. Alternatively, copy the hex file found in the Zephyr output directory (/build/zephyr).

### VSCode Setup (Optional)
1. Make sure you have followed the steps under General Setup and Building the App.
2. Make sure the [C/C++ extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools) is installed in VSCode.
3. Edit the workspace file located in the root of this project so that the path for the 'Zephyr' folder is the full path to the root Zephyr directory. 
4. Use VSCode to open the workspace file and wait for things to index.

### Advanced Flashing (Device-Specific Data)
The applications support loading application configs from on-device non-volatile flash memory. The data
is expected to follow a particular format - most notably, it's expected to exist at the first page
in flash that contains no application data (if such a page exists). The prototype key-generation tool
found [here](https://github.com/ubc-systopia/pancast-keys) is set up to prepare hex files that conform
to this spec. A little work must be done to combine application and configuration data into a single,
flashable program:
1. First, compile the application (without configs) as desired. Make sure the application FLASH_OFFSET
parameters are set correctly. We will assume the output lives in `zephyr.hex`.
2. Generate the desired config hex file for the device, call this something like `config.hex`. This can be done easily using the key-generation program. (NOTE: make sure the generated config device type matches the application being used).
3. Finally, use something like the following command to combine the data:
```
mergehex -m zephyr.hex config.hex -o pancast.hex
```
