#ifndef STORAGE__H
#define STORAGE__H

#include "./dongle.h"

#include <drivers/flash.h>

#define FLASH_WORD_SIZE 8
#define FLASH_OFFSET 0x21000

typedef off_t storage_addr_t;

typedef struct
{
    storage_addr_t config;
    storage_addr_t otp;
    storage_addr_t log;
    enctr_entry_counter_t enctr_entries; // number of entries
} _dongle_storage_map_;

typedef struct
{
    const struct device *dev;
    size_t min_block_size;
    int num_pages;
    size_t page_size;
    _dongle_storage_map_ map;
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

// LOAD OTP
// Should read the data associated with the ith OTP code into
// the container.
void dongle_storage_load_otp(dongle_storage *sto, int i, dongle_otp_t *otp);

enctr_entry_counter_t dongle_storage_num_encounters(dongle_storage *sto);

// LOAD ENCOUNTER
// API is defined using a callback structure
typedef int (*dongle_encounter_cb)(enctr_entry_counter_t i, dongle_encounter_entry *entry);

// load function iterates through the records, and calls cb for each
// variable i is provided as the index into the log
void dongle_storage_load_encounter(dongle_storage *sto,
                                   enctr_entry_counter_t i, dongle_encounter_cb cb);

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