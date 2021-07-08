#ifndef STORAGE__H
#define STORAGE__H

// Non-Volatile storage
// Defines a simple API to read and write specific data structures
// with minimal coupling the underlying drivers.

#include "dongle.h"

#include <stddef.h>

// Reflects the total size of the entry in storage while taking
// minimum alignment into account.
#define ENCOUNTER_ENTRY_SIZE (sizeof(beacon_location_id_t) + sizeof(beacon_id_t) + \
                              sizeof(beacon_timer_t) + sizeof(dongle_timer_t) +    \
                              BEACON_EPH_ID_SIZE)

#ifdef DONGLE_PLATFORM__ZEPHYR
#define FLASH_OFFSET 0x2D000
#else
#define FLASH_OFFSET 0x2e000
#endif

// Upper-bound of size of encounter log, in bytes
#define TARGET_FLASH_LOG_SIZE 0x100

// Maximum number of encounters stored at one time
#define MAX_LOG_COUNT (TARGET_FLASH_LOG_SIZE / ENCOUNTER_ENTRY_SIZE)

// Maximum number of bytes used to store encounters
// This is multiple of the encounter size
// And assumes the size is block-aligned
#define FLASH_LOG_SIZE (ENCOUNTER_ENTRY_SIZE * MAX_LOG_COUNT)

#ifdef DONGLE_PLATFORM__ZEPHYR
#include <drivers/flash.h>
typedef off_t storage_addr_t;
typedef const struct device flash_device_t;
#else
#include "em_msc.h"
typedef uint32_t storage_addr_t;
#endif

typedef struct
{
    //enctr_entry_counter_t tail; // index of oldest stored recent encounter
    enctr_entry_counter_t head; // index to write next stored encounter
} _encounter_storage_cursor_;

typedef struct
{
    storage_addr_t config;  // address of device configuration
    storage_addr_t otp;     // address of OTP storage
    storage_addr_t log;     // address of first log entry
    storage_addr_t log_end; // address of next available space after log
} _dongle_storage_map_;

typedef struct
{
#ifdef DONGLE_PLATFORM__ZEPHYR
    flash_device_t *dev;
#else
    MSC_ExecConfig_TypeDef mscExecConfig;
#endif
    size_t min_block_size;
    int num_pages;
    size_t page_size;
    _dongle_storage_map_ map;
    _encounter_storage_cursor_ encounters;
    storage_addr_t off; // flash offset
    uint64_t numErasures;
} dongle_storage;

// STORAGE INIT
// Should initialize the internal storage representation,
// including system-level information. Application configs like
// the storage map are left partially uninitialized.
// This allows *load_config* to be used.
void dongle_storage_init(dongle_storage *sto);

// LOAD CONFIG
// Should load application configuration data into the provided
// container, and set the map to allow load_otp to be used.
void dongle_storage_load_config(dongle_storage *sto, dongle_config_t *cfg);

// Write an existing config to storage
void dongle_storage_save_config(dongle_storage *sto, dongle_config_t *cfg);

// LOAD OTP
// Should read the data associated with the ith OTP code into
// the container.
void dongle_storage_load_otp(dongle_storage *sto, int i, dongle_otp_t *otp);

// Container for OTPs loaded during testing
typedef dongle_otp_t otp_set[NUM_OTP];

// Save a pre-determined list of OTPs
void dongle_storage_save_otp(dongle_storage *sto, otp_set otps);

// Determine if a given otp is used
int otp_is_used(dongle_otp_t *otp);

// If an unused OTP exists with the given value, returns its index
// and marks the code as used. Otherwise returns a negative value
int dongle_storage_match_otp(dongle_storage *sto, uint64_t val);

// Determine the number of encounters currently logged
enctr_entry_counter_t dongle_storage_num_encounters(dongle_storage *sto);

// LOAD ENCOUNTER
// API is defined using a callback structure
typedef int (*dongle_encounter_cb)(enctr_entry_counter_t i, dongle_encounter_entry *entry);

// load function iterates through the records, and calls cb for each
// variable i is provided as the index into the log
void dongle_storage_load_encounter(dongle_storage *sto,
                                   enctr_entry_counter_t i, dongle_encounter_cb cb);

void dongle_storage_load_all_encounter(dongle_storage *sto, dongle_encounter_cb cb);

void dongle_storage_load_single_encounter(dongle_storage *sto,
                                          enctr_entry_counter_t i, dongle_encounter_entry *);

// WRITE ENCOUNTER
void dongle_storage_log_encounter(dongle_storage *sto,
                                  beacon_location_id_t *loc,
                                  beacon_id_t *beacon_id,
                                  beacon_timer_t *beacon_time,
                                  dongle_timer_t *dongle_time,
                                  beacon_eph_id_t *eph_id);

#define DONGLE_STORAGE_MAX_PRINT_LEN 64

int dongle_storage_print(dongle_storage *, storage_addr_t, size_t);

#endif
