#ifndef STORAGE__H
#define STORAGE__H

// Non-Volatile storage
// Defines a simple API to read and write specific data structures
// with minimal coupling the underlying drivers.

#include "beacon.h"

#include <stddef.h>

// Offset determines a safe point of read/write beyond the pages used by application
// binaries. For now, determined empirically by doing a compilation pass then adjusting
// the value
#define FLASH_OFFSET 0x30000

#ifdef BEACON_PLATFORM__ZEPHYR
#include <drivers/flash.h>
typedef off_t storage_addr_t;
typedef const struct device flash_device_t;
#else
#include "em_msc.h"
typedef uint32_t storage_addr_t;
#endif

typedef struct
{
    storage_addr_t config;  // address of device configuration
    storage_addr_t stat;    // address of saved statistics
} _beacon_storage_map_;

typedef struct
{
#ifdef BEACON_PLATFORM__ZEPHYR
    flash_device_t *dev;
#else
    MSC_ExecConfig_TypeDef mscExecConfig;
#endif
    size_t min_block_size;
    int num_pages;
    size_t page_size;
    storage_addr_t total_size;
    _beacon_storage_map_ map;
    storage_addr_t off; // flash offset
    uint64_t numErasures;
} beacon_storage;

// STORAGE INIT
// Should initialize the internal storage representation,
// including system-level information. Application configs like
// the storage map are left partially uninitialized.
// This allows *load_config* to be used.
void beacon_storage_init(beacon_storage *sto);

// LOAD CONFIG
// Should load application configuration data into the provided
// container, and set the map to allow load_otp to be used.
void beacon_storage_load_config(beacon_storage *sto, beacon_config_t *cfg);

void beacon_storage_save_stat(beacon_storage *sto, void * stat, size_t len);
void beacon_storage_read_stat(beacon_storage *sto, void * stat, size_t len);

#endif
