# Nordic beacon

PanCast BLE Beacon implementation based on the Nordic NRF52832 board.

## Getting started

1. Make sure you have the Zephyr project cloned to a location on the development machine, and have followed the setup documentation [here](https://docs.zephyrproject.org/latest/getting_started/index.html) (in particular, you should have the `west` command available).

## Compiling the application

1. Navigate to the root Zephyr directory (the one containing the samples directory)
2. Issue the following command: `west build -p auto -b nrf52dk_nrf52832 <app_path> -- -Wno-dev -DCMAKE_EXPORT_COMPILE_COMMANDS=ON` where `<app_path>` is the full path to the application to be built (For example: `$HOME/projects/pancast-code/ble-encounters/beacon`).

Alternatively, invoke `zephyr-beacon/scripts/build.sh`.

### Troubleshooting
1. On Apple M1, `west flash` JLink dll may not be recognized properly:

```
ERROR: JLinkARM DLL load failed. Try again. If it keeps failing, please
ERROR: reinstall latest JLinkARM from Segger webpage.
NOTE: For additional output, try running again with logging enabled (--log).
NOTE: Any generated log error messages will be displayed.
```

- Download and install JLink from [SEGGER](https://www.segger.com/downloads/jlink/) for x86\_64 (Intel) target for Mac. (You may need to use [Rosetta](https://support.apple.com/en-ca/HT211861), or minimally switch the architecture to [x86\_64](https://vineethbharadwaj.medium.com/m1-mac-switching-terminal-between-x86-64-and-arm64-e45f324184d9) for installing JLink).

## Flashing the application

1. Make sure the development board is plugged in.
2. In the root Zephyr directory, run:   `west flash`, to flash the currently built application. Alternatively, can directly run `zephyr-beacon/scripts/flash.sh`

To customize the built code image with different device IDs, follow the
instructions from
https://github.com/ubc-systopia/pancast-keys/blob/master/README.md.

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

