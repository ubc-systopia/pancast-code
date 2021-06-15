#define LOG_LEVEL__INFO

#include "./storage.h"

#include <drivers/flash.h>

#include "../../common/src/log.h"
#include "../../common/src/util.h"

#define prev_multiple(k, n) ((n) - ((n) % (k)))
#define next_multiple(k, n) ((n) + ((k) - ((n) % (k)))) // buggy
#define align(size) off = next_multiple(size, off)

off_t off; // flash offset

static bool _flash_page_info_(const struct flash_pages_info *info, void *data)
{
#define sto ((dongle_storage *)data)
    if (!sto->num_pages)
    {
        sto->page_size = info->size;
    }
    else if (info->size != sto->page_size)
    {
        log_errorf("differing page sizes! (%u and %u)\n", info->size,
                   sto->page_size);
    }
    sto->num_pages++;
    return true;
#undef sto
}

#define st (*sto)

#define block_align align(st.min_block_size)
#define block_pad(size) next_multiple(st.min_block_size, size)
#define padded_sizeof(t) block_pad(sizeof(t))

#define erase(offset)                              \
    log_infof("erasing page at 0x%x\n", (offset)), \
        flash_erase(st.dev, (offset), st.page_size);

void pre_erase(dongle_storage *sto, size_t write_size)
{
// Erase before write
#define page_num(o) ((o) / st.page_size)
    if ((off % st.page_size) == 0)
    {
        erase(off);
    }
    else if (page_num(off + write_size) > page_num(off))
    {
#undef page_num
        erase(next_multiple(st.page_size, off));
    }
}

int _flash_read_(dongle_storage *sto, void *data, size_t size)
{
    log_debugf("reading %d bytes from flash at address 0x%x\n", size, off);
    return flash_read(st.dev, off, data, size);
}

int _flash_write_(dongle_storage *sto, void *data, size_t size)
{
    log_debugf("writing %d bytes to flash at address 0x%x\n", size, off);
    return flash_write(st.dev, off, data, size)
           ? log_error("Error writing flash\n"),
           1 : 0;
}

void dongle_storage_init(dongle_storage *sto)
{
    log_info("Initializing storage\n");
    off = 0;
    st.map.enctr_entries = 0;
    st.dev = device_get_binding(DT_CHOSEN_ZEPHYR_FLASH_CONTROLLER_LABEL);
    log_info("Getting flash information.\n");
    st.min_block_size = flash_get_write_block_size(st.dev);
    st.num_pages = 0;
    flash_page_foreach(st.dev, _flash_page_info_, sto);
    log_infof("Pages: %d, size=%u\n", st.num_pages, st.page_size);
    st.map.config = FLASH_OFFSET;
}

#define cf (*cfg)

void dongle_storage_load_config(dongle_storage *sto, dongle_config_t *cfg)
{
    log_info("Loading config\n");
    off = st.map.config;
#define read(size, dst) (_flash_read_(sto, dst, size), off += size)
    read(sizeof(dongle_id_t), &cf.id);
    read(sizeof(dongle_timer_t), &cf.t_init);
    read(sizeof(key_size_t), &cf.backend_pk_size);
    read(PK_MAX_SIZE, &cf.backend_pk);
    read(sizeof(key_size_t), &cf.dongle_sk_size);
    read(SK_MAX_SIZE, &cf.dongle_sk);
    align(FLASH_WORD_SIZE);
    st.map.otp = off;
    // log start is pushed onto the next blank page
    st.map.log = next_multiple(st.page_size,
                               st.map.otp + (NUM_OTP * sizeof(dongle_otp_t)));
#undef read
}

void dongle_storage_save_config(dongle_storage *sto, dongle_config_t *cfg)
{
    log_debug("Saving config\n");
    off = st.map.config;
#define write(data, size) (pre_erase(sto, size), _flash_write_(sto, data, size), off += size)
    write(&cf.id, sizeof(dongle_id_t));
    write(&cf.t_init, sizeof(dongle_timer_t));
    write(&cf.backend_pk_size, sizeof(key_size_t));
    write(&cf.backend_pk, PK_MAX_SIZE);
    write(&cf.dongle_sk_size, sizeof(key_size_t));
    write(&cf.dongle_sk, SK_MAX_SIZE);
#undef write
}

#undef cf

#define OTP(i) (st.map.otp + (i * sizeof(dongle_otp_t)))

void dongle_storage_load_otp(dongle_storage *sto, int i, dongle_otp_t *otp)
{
    off = OTP(i), _flash_read_(sto, otp, sizeof(dongle_otp_t));
}

void dongle_storage_save_otp(dongle_storage *sto, otp_set otps)
{
    log_debug("Saving OTPs\n");
    for (int i = 0; i < NUM_OTP; i++)
    {
        off = OTP(i);
        pre_erase(sto, sizeof(dongle_otp_t));
        _flash_write_(sto, &otps[i], sizeof(dongle_otp_t));
    }
}

int dongle_storage_match_otp(dongle_storage *sto, uint64_t val)
{
    dongle_otp_t otp;
    for (int i = 0; i < NUM_OTP; i++)
    {
        dongle_storage_load_otp(sto, i, &otp);
        if (otp.val == val)
        {
            otp.flags |= 0x0000000000000001;
            off = OTP(i),
            _flash_write_(sto, &otp, sizeof(dongle_otp_t));
            return i;
        }
    }
    return -1;
}

enctr_entry_counter_t dongle_storage_num_encounters(dongle_storage *sto)
{
    return st.map.enctr_entries;
}

// Reflects the total size of the entry in storage while taking
// minimum alignment into account.
#define ENCOUNTER_ENTRY_SIZE                                                          \
    next_multiple(FLASH_WORD_SIZE,                                                    \
                  padded_sizeof(beacon_location_id_t) + padded_sizeof(beacon_id_t) +  \
                      padded_sizeof(beacon_timer_t) + padded_sizeof(dongle_timer_t) + \
                      block_pad(BEACON_EPH_ID_SIZE))

//#define ENCOUNTER_LOG_BASE (ENCOUNTER_BASE + sizeof(flash_check_t))
#define ENCOUNTER_LOG_OFFSET(i) (st.map.log + (i * ENCOUNTER_ENTRY_SIZE))

void dongle_storage_load_encounter(dongle_storage *sto,
                                   enctr_entry_counter_t i, dongle_encounter_cb cb)
{
    dongle_encounter_entry en;
    if (i >= st.map.enctr_entries)
    {
        log_errorf("Starting index for encounter log (%llu) is too large\n", i);
    }
    log_infof("loading log entries starting at index %llu\n", i);
    do
    {
        if (i < st.map.enctr_entries)
        {
            off = ENCOUNTER_LOG_OFFSET(i);
#define read(size, dst) _flash_read_(sto, dst, size), off += size, block_align
            read(sizeof(beacon_id_t), &en.beacon_id);
            read(sizeof(beacon_location_id_t), &en.location_id);
            read(sizeof(beacon_timer_t), &en.beacon_time);
            read(sizeof(dongle_timer_t), &en.dongle_time);
            read(BEACON_EPH_ID_SIZE, &en.eph_id);
            align(FLASH_WORD_SIZE);
#undef read
        }
        else
        {
            break;
        }
        i++;
    } while (cb(i - 1, &en));
}

void dongle_storage_log_encounter(dongle_storage *sto,
                                  beacon_location_id_t *location_id,
                                  beacon_id_t *beacon_id,
                                  beacon_timer_t *beacon_time,
                                  dongle_timer_t *dongle_time,
                                  beacon_eph_id_t *eph_id)
{
    storage_addr_t start = ENCOUNTER_LOG_OFFSET(st.map.enctr_entries);
    off = start;
    log_debugf("write log; existing entries: %llu, offset: 0x%x\n", st.map.enctr_entries, off);
    pre_erase(sto, ENCOUNTER_ENTRY_SIZE);
#define write(data, size) \
    _flash_write_(sto, data, size), off += size, block_align
    write(beacon_id, sizeof(beacon_id_t));
    write(location_id, sizeof(beacon_location_id_t));
    write(beacon_time, sizeof(beacon_timer_t));
    write(dongle_time, sizeof(dongle_timer_t));
    write(eph_id, BEACON_EPH_ID_SIZE);
    log_debugf("offset after log write: 0x%x\n", off);
    align(FLASH_WORD_SIZE);
    log_debugf("offset after alignment: 0x%x\n", off);
    log_debugf("total size: %u (entry size=%d)\n", off - start, ENCOUNTER_ENTRY_SIZE);
#undef write
    st.map.enctr_entries++;
    log_debugf("log now contains %llu entries\n", st.map.enctr_entries);
}

int dongle_storage_print(dongle_storage *sto, storage_addr_t addr, size_t len)
{
    if (len > DONGLE_STORAGE_MAX_PRINT_LEN)
    {
        log_error("Cannot print that many bytes of flash");
        return 1;
    }
    uint8_t data[DONGLE_STORAGE_MAX_PRINT_LEN];
    off = addr;
    _flash_read_(sto, data, len);
    print_bytes(data, len, "Flash data");
    return 0;
}

#undef block_align
#undef st
#undef align
#undef next_multiple
#undef prev_multiple