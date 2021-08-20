# PanCast Raspberry Pi Serial Transfer

The Client app is to be run on a Raspberry Pi. The client will fetch risk broadcast data periodically from the backend and send the data to the Silicon Labs 
Bluetooth application for broadcast over BLE.

## Getting Started

Follow the instruction in Raspberry Pi Set-up Doc: https://docs.google.com/document/d/1yTDDE8dWmT4W_3zhqdPPBc3t0FCl_lEvqI6VNVbjvZs/edit (TODO: move instructions to README)

## Compiling the Application

1. Navigate to `src` directory and use `make` to compile the app.
`
## Running the Application

1. Connect the GPIO pins on the raspberry pi to the EXT header pins on the Silicon Labs board using jumper wires. Connect ground to ground and PB00 on 
    Silicon Labs EXT header to GPIO24 on the Raspberry Pi. 
2. Make sure that the broadcast device (Silicon Labs board) is connected to the Raspberry Pi via USB and that both devices are on. 
3. Run the application using `sudo ./client` from the `src` directory where the app has been built.
