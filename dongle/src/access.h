#ifndef DONGLE_ACCESS__H
#define DONGLE_ACCESS__H

// TERMINAL INTERACTION
// This is a connection-oriented protocol for external upload
// through a user-interface device. Dongle acts as the peripheral
// and accepts connections while proceeding with normal operation.
// Only the 'delayed-release' mechanism is supported right now.
// Data is currently unencrypted when sent to the terminal.

#include <stdint.h>

#include "./dongle.h"
#include "./storage.h"

// SERVICE UUID: e7f72a03-803b-410a-98d4-4be5fad8e217
#define DONGLE_INTERACT_SERVICE_ID_0 0xe7f72a03
#define DONGLE_INTERACT_SERVICE_ID_1 0x803b
#define DONGLE_INTERACT_SERVICE_ID_2 0x410a
#define DONGLE_INTERACT_SERVICE_ID_3 0x98d4
#define DONGLE_INTERACT_SERVICE_ID_4 0x4be5fad8e217

// Macro for expressing indexed UUIDS for use in the access protocol
// These are based on the primary service ID (index 0) and support
// a large number of IDs - assuming that the 6-byte part of the base
// UUID is well below the max val.
#define DONGLE_INTERACT_UUID(i) BT_UUID_128_ENCODE( \
    DONGLE_INTERACT_SERVICE_ID_0,                   \
    DONGLE_INTERACT_SERVICE_ID_1,                   \
    DONGLE_INTERACT_SERVICE_ID_2,                   \
    DONGLE_INTERACT_SERVICE_ID_3,                   \
    DONGLE_INTERACT_SERVICE_ID_4 + i)

#define DONGLE_SERVICE_UUID DONGLE_INTERACT_UUID(0)

#define DONGLE_CHARACTERISTIC_UUID DONGLE_INTERACT_UUID(1)

// Data types
#define DONGLE_UPLOAD_DATA_TYPE_OTP 0x01
#define DONGLE_UPLOAD_DATA_TYPE_NUM_RECS 0x02
#define DONGLE_UPLOAD_DATA_TYPE_ACK_NUM_RECS 0x03
#define DONGLE_UPLOAD_DATA_TYPE_DATA_0 0x04
#define DONGLE_UPLOAD_DATA_TYPE_ACK_DATA_0 0x05
#define DONGLE_UPLOAD_DATA_TYPE_DATA_1 0x06
#define DONGLE_UPLOAD_DATA_TYPE_ACK_DATA_1 0x07
#define DONGLE_UPLOAD_DATA_TYPE_DATA_2 0x08
#define DONGLE_UPLOAD_DATA_TYPE_ACK_DATA_2 0x09
#define DONGLE_UPLOAD_DATA_TYPE_DATA_3 0x0a
#define DONGLE_UPLOAD_DATA_TYPE_ACK_DATA_3 0x0b
#define DONGLE_UPLOAD_DATA_TYPE_DATA_4 0x0c
#define DONGLE_UPLOAD_DATA_TYPE_ACK_DATA_4 0x0d

typedef union
{
    uint8_t flags;
    struct
    {
        uint8_t _;
        uint8_t val[sizeof(dongle_otp_val)];
    } otp;
    struct
    {
        uint8_t _;
        uint8_t val[sizeof(enctr_entry_counter_t)];
    } num_recs;
    struct
    {
        uint8_t _;
        uint8_t bytes[19];
    } data;
} interact_state;

int access_advertise();
void peer_update();
void interact_update();
dongle_storage *get_dongle_storage();

#endif