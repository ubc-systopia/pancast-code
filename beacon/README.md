# PanCast Dongle

PanCast Beacon / Network Beacon Implementation based on the Silicon Labs Gecko SDK platform. Currently compatible with the `EFR32BG22` system - tested on `Wireless Starter Kit Mainboard (BRD4001A Rev A01)` with
`EFR32xG22 2.4 GHz 6 dBm Radio Board (BRD4182A Rev B03)`.

## Getting Started

1. Make sure you have followed the SiLabs Setup documentation (linked in the main README) and that your ARM Toolchain version is 2019-q4 (aka `v9.2.1`).
2. In the Simplicity Studio IDE, select the option to import a project (`File > Import` on the Mac version).
3. Browse to select the directory containing this README, and click next.
4. Click next to skip the Build Configurations page.
5. Set the project name to 'pancast-beacon'.
6. Uncheck 'Use Default Location', and once again browse to select this directory. Click Finish

## Compiling the Application

1. Right-click the project in the project view, and select 'Build Project'.

## Flashing the Application

1. Right-click the project in the project view, and select 'Run As'. Choose 'Silicon Labs ARM Program'.