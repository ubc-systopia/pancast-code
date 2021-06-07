#ifndef TEST__H
#define TEST__H

#include "./pancast.h"


#define TEST_BEACON_ID		    0x01234567
#define TEST_BEACON_LOC_ID	    0x0123456789abcdef
#define TEST_BEACON_INIT_TIME   0x00000000

#define TEST_BEACON_BACKEND_KEY_SIZE        6

static pubkey_t TEST_BACKEND_PK = {{
    0xad , 0xf4 , 0xca , 0x6c , 0xa6 , 0xd9
}};

#define TEST_BEACON_SK_SIZE                 16

static beacon_sk_t TEST_BEACON_SK = {{
    0xcb , 0x43 , 0xf7 , 0x56 , 0x16 , 0x25 , 0xb3 , 0xd0 , 0xd0 , 0xbe ,
    0xad , 0xf4 , 0xca , 0x6c , 0xa6 , 0xd9
}};

#endif