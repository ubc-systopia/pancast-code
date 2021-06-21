#ifndef DONGLE_ACCESS__H
#define DONGLE_ACCESS__H

#include <stdint.h>

#include "./dongle.h"
#include "./storage.h"

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