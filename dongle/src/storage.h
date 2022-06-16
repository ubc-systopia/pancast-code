#ifndef STORAGE__H
#define STORAGE__H

// Non-Volatile storage
// Defines a simple API to read and write specific data structures
// with minimal coupling the underlying drivers.

#include "dongle.h"
#include "common/src/settings.h"
#include "common/src/platform/gecko.h"
#include "config/nvm3_default_config.h"

#include <stddef.h>

// Reflects the total size of the entry in storage while taking
// minimum alignment into account.
#define ENCOUNTER_ENTRY_SIZE sizeof(dongle_encounter_entry_t)

#define FLASH_OFFSET 0x60000

#define NVM_OFFSET 0x78000
#define NVM_SIZE NVM3_DEFAULT_NVM_SIZE

/*
 * storage address of encounter log
 */
#define ENCOUNTER_LOG_START FLASH_OFFSET
#define ENCOUNTER_LOG_END   NVM_OFFSET

/*
 * physical addr of an encounter entry in flash:
 * flash page number + offset in page
 */
#define ENCOUNTER_LOG_OFFSET(sto, j) \
    (ENCOUNTER_LOG_START +  \
     (((j) / ENCOUNTERS_PER_PAGE) * FLASH_DEVICE_PAGE_SIZE) + \
     (((j) % ENCOUNTERS_PER_PAGE) * ENCOUNTER_ENTRY_SIZE))

/*
 * storage address for device configuration
 */
#define DONGLE_CONFIG_OFFSET  \
  ((FLASH_DEVICE_NUM_PAGES - 1) * FLASH_DEVICE_PAGE_SIZE)

/*
 * storage address for OTPs
 */
#define DONGLE_OTPSTORE_OFFSET  \
  (DONGLE_CONFIG_OFFSET + sizeof(dongle_id_t) + (2*sizeof(dongle_timer_t)) +  \
  (2*sizeof(key_size_t)) + PK_MAX_SIZE + SK_MAX_SIZE + \
  (2*sizeof(enctr_entry_counter_t)))

/*
 * storage address for stats object
 */
#define DONGLE_STATSTORE_OFFSET \
  (DONGLE_OTPSTORE_OFFSET + (NUM_OTP*sizeof(dongle_otp_t)))

/*
 * space available for encounter log (in bytes)
 */
#define TARGET_FLASH_LOG_SIZE (NVM_OFFSET - FLASH_OFFSET)

/*
 * number of flash pages for encounter log
 */
#define TARGET_FLASH_LOG_NUM_PAGES  \
  (TARGET_FLASH_LOG_SIZE / FLASH_DEVICE_PAGE_SIZE)

/*
 * number of encounters that can be stored per flash page
 * no encounters straddle pages
 */
#define ENCOUNTERS_PER_PAGE (FLASH_DEVICE_PAGE_SIZE / ENCOUNTER_ENTRY_SIZE)

/*
 * max number of encounters that can be stored in a dongle
 */
#define MAX_LOG_COUNT (TARGET_FLASH_LOG_NUM_PAGES * ENCOUNTERS_PER_PAGE)

#include "em_msc.h"
typedef uint32_t storage_addr_t;

typedef struct {
  enctr_entry_counter_t tail; // index of oldest stored recent encounter
  enctr_entry_counter_t head; // index to write next stored encounter
} _encounter_storage_cursor_;

typedef struct {
  _encounter_storage_cursor_ encounters;
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

#if 0
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
#endif

// Determine the number of encounters currently logged
enctr_entry_counter_t dongle_storage_num_encounters_current(dongle_storage *sto);
enctr_entry_counter_t num_encounters_current(enctr_entry_counter_t head,
    enctr_entry_counter_t tail);

// LOAD ENCOUNTER
// API is defined using a callback structure
typedef int (*dongle_encounter_cb)(enctr_entry_counter_t i,
		dongle_encounter_entry_t *entry);

// load function iterates through the records, and calls cb for each
// variable i is provided as the index into the log
void dongle_storage_load_encounter(dongle_storage *sto,
    enctr_entry_counter_t i, dongle_encounter_cb cb);

void dongle_storage_load_single_encounter(dongle_storage *sto,
    enctr_entry_counter_t i, dongle_encounter_entry_t *);

#if 0
void dongle_storage_load_encounters_from_time(dongle_storage *sto,
    dongle_timer_t min_time, dongle_encounter_cb cb);
#endif

// WRITE ENCOUNTER
void dongle_storage_log_encounter(dongle_storage *sto, dongle_config_t *cfg,
		dongle_timer_t *dongle_time, dongle_encounter_entry_t *en);

void dongle_storage_save_stat(dongle_storage *sto, dongle_config_t *cfg,
    void * stat, size_t len);
void dongle_storage_read_stat(dongle_storage *sto, void * stat, size_t len);
void dongle_storage_info(dongle_storage *);

void dongle_storage_clean_log(dongle_storage *sto, dongle_timer_t cur_time);

#endif
