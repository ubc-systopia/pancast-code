# Bluetooth LE Encounters
An implementation of the beacon to dongle advertisement protocol for encounter tracing.

## Structure
Application code is found in the `common`, `beacon`, and `dongle` directories, under `src`. 
The latter two are the applications and rely on the Zephyr project stack. 
See the following section for details.

## Development
### General Setup
1. Make sure you have the Zephyr project cloned to a location on the development machine, and have followed the setup documentation [here](https://docs.zephyrproject.org/latest/getting_started/index.html) (in particular, you should have the `west` command available).

### Building the App
1. Navigate to the root Zephyr directory (the one containing the samples directory)
2. Issue the following command: `west build -p auto -b nrf52dk_nrf52832 <app_path> -- -Wno-dev -DCMAKE_EXPORT_COMPILE_COMMANDS=ON` where `<app_path>` is the full path to the application to be built (For example: '$HOME/projects/ble-encounters/beacon').

### Flashing the App
1. Make sure the development board is plugged in.
2. In the root Zephyr directory, run:   `west flash`, to flash the currently built application.

### VSCode Setup (Optional)
1. Make sure you have followed the steps under General Setup and Building the App.
2. Make sure the [C/C++ extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools) is installed in VSCode.
3. Edit the workspace file located in the root of this project so that the path for the 'Zephyr' folder is the full path to the root Zephyr directory. 
4. Use VSCode to open the workspace file and wait for things to index.