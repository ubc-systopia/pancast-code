# Bluetooth LE Encounters
An implementation of the beacon to dongle advertisement protocol for encounter tracing.

## Structure
Application code is found in the `src` directory. This relies on the Zephyr project stack. See the following section for details.

## Development
### General Setup
1. Make sure you have the Zephyr project cloned to a location on the development machine, and have followed the setup documentation (in particular you should have the `west` command available).

### Building the App
1. Navigate to the root Zephyr directory (the one containing the samples directory)
2. Issue the following command: `west build -p auto -b nrf52dk_nrf52832 <app_path> -- -Wno-dev -DCMAKE_EXPORT_COMPILE_COMMANDS=ON` where `<app_path>` is the full path to the directory containing this README.

### Flashing the App
1. Make sure the development board is plugged in.
2. In the root Zephyr directory, run:   `west flash`.

### VSCode Setup
1. Make sure you have followed the steps under General Setup and Building the App.
2. Make sure the C/C++ extension is installed in VSCode.
3. Edit the workspace file located in the root of this project so that the path for the 'Zephyr' folder is the full path to the root Zephyr directory. 
4. Use VSCode to open the workspace file and wait for things to index.
