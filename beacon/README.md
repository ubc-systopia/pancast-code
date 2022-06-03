# SiLabs beacon

PanCast Beacon / Network Beacon Implementation based on the Silicon Labs Gecko SDK platform. Currently compatible with the `EFR32BG22` system - tested on `Wireless Starter Kit Mainboard (BRD4001A Rev A01)` with
`EFR32xG22 2.4 GHz 6 dBm Radio Board (BRD4182A Rev B03)`.

## Getting started

1. Make sure you have followed the SiLabs Setup documentation (linked in the main README) and that your ARM Toolchain version is 10-2020-q4-major (aka `v10.2.1`).
2. In the Simplicity Studio IDE, select the option to import a project (`File > Import` on the Mac version).
3. Browse to select the directory containing this README, and click next.
4. Click next to skip the Build Configurations page.
5. Set the project name to 'pancast-beacon'.
6. Uncheck 'Use Default Location', and once again browse to select this directory. Click Finish

## Compiling the application

1. Right-click the project in the project view, and select 'Build Project'.

## Flashing the application

1. Right-click the project in the project view, and select 'Run As'. Choose 'Silicon Labs ARM Program'.

### Advanced flashing (device-specific data)
1. Follow steps 1 and 2 above (using Simplicity Studio for the build).
2. Combine the hex files using the following command: `mergehex -m <build_dir>/<device>.hex config.hex -o app.hex`. Where `build_dir` is the build directory e.g. `GNU ARM v10.2.1 - Default` and `device` is the name of a device application (e.g. `pancast-dongle` or `pancast-beacon`).
3. In Simplicity Studio, open Flash Programmer (the blue, downward-facing arrow button in the toolbar).
4. Select the correct board if needed.
5. In the File section, browse to select the `app.hex` file you just generated.
6. Erase, then program. Close the window. The device is now flashed.

### Advanced flashing (using CLI)
To customize the code image with different device IDs, follow the instructions
from https://github.com/ubc-systopia/pancast-keys/blob/master/README.md.
