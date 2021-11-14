# Nordic beacon

PanCast Beacon / Network Beacon Implementation based on the Silicon Labs Gecko SDK platform. Currently compatible with the `EFR32BG22` system - tested on `Wireless Starter Kit Mainboard (BRD4001A Rev A01)` with
`EFR32xG22 2.4 GHz 6 dBm Radio Board (BRD4182A Rev B03)`.

## Getting started

1. Make sure you have the Zephyr project cloned to a location on the development machine, and have followed the setup documentation [here](https://docs.zephyrproject.org/latest/getting_started/index.html) (in particular, you should have the `west` command available).

## Compiling the application

1. Navigate to the root Zephyr directory (the one containing the samples directory)
2. Issue the following command: `west build -p auto -b nrf52dk_nrf52832 <app_path> -- -Wno-dev -DCMAKE_EXPORT_COMPILE_COMMANDS=ON` where `<app_path>` is the full path to the application to be built (For example: `$HOME/projects/pancast-code/ble-encounters/beacon`).

## Flashing the application

1. Make sure the development board is plugged in.
2. In the root Zephyr directory, run:   `west flash`, to flash the currently built application.

#### VSCode Setup (Optional)
1. Make sure you have followed the steps under General Setup and Building the App.
2. Make sure the [C/C++ extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools) is installed in VSCode.
3. Edit the workspace file located in the root of this project so that the path for the 'Zephyr' folder is the full path to the root Zephyr directory. 
4. Use VSCode to open the workspace file and wait for things to index.

### Advanced flashing (device-specific data)
The applications support loading application configs from on-device non-volatile flash memory. The data
is expected to follow a particular format -- most notably, it's expected to exist at the first page
in flash that contains no application data (if such a page exists). The prototype key-generation tool
found [here](https://github.com/ubc-systopia/pancast-keys) is set up to prepare hex files that conform
to this spec. A little work must be done to combine application and configuration data into a single,
flashable program:

1. First, compile the application (without configs) as desired. Make sure the application FLASH_OFFSET
macro is set correctly in the code (this may require some investigation into the generated hex file).
2. Generate the desired config hex file for the device, call this something like `config.hex`. This can be done easily using the key-generation program. (NOTE: make sure the generated config device type matches the application being used).
3. Finally, use something like the following command to combine the data (assume the compilation output is `zephyr.hex`):
```
mergehex -m zephyr.hex config.hex -o app.hex
```

