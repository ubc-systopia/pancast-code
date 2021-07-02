#define LOG_LEVEL__INFO

#include "storage.h"

#include "../../common/src/log.h"
#include "../../common/src/util.h"

#define prev_multiple(k, n) ((n) - ((n) % (k)))
#define next_multiple(k, n) ((n) + ((k) - ((n) % (k)))) // buggy

storage_addr_t off; // flash offset

#ifdef DONGLE_PLATFORM__ZEPHYR
static bool _flash_page_info_(const struct flash_pages_info *info, void *data)
{
#define sto ((dongle_storage *)data)
    if (!sto->num_pages)
    {
        sto->page_size = info->size;
    }
    else if (info->size != sto->page_size)
    {
        log_errorf("differing page sizes! (%u and %u)\r\n", info->size,
                   sto->page_size);
    }
    sto->num_pages++;
    return true;
#undef sto
}
#endif

#define st (*sto)

void dongle_storage_erase(dongle_storage *sto, storage_addr_t offset)
{
    log_debugf("erasing page at 0x%x\r\n", (offset));
#ifdef DONGLE_PLATFORM__ZEPHYR
    flash_erase(st.dev, (offset), st.page_size);
#else
    DONGLE_NO_OP;
#endif
}

#define erase(addr) dongle_storage_erase(sto, addr)

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
    log_debugf("reading %d bytes from flash at address 0x%x\r\n", size, off);
#ifdef DONGLE_PLATFORM__ZEPHYR
    return flash_read(st.dev, off, data, size);
#else
    DONGLE_NO_OP;
    return 0;
#endif
}

int _flash_write_(dongle_storage *sto, void *data, size_t size)
{
    log_debugf("writing %d bytes to flash at address 0x%x\r\n", size, off);
#ifdef DONGLE_PLATFORM__ZEPHYR
    return flash_write(st.dev, off, data, size)
           ? log_error("Error writing flash\r\n"),
           1 : 0;
#else
    DONGLE_NO_OP;
    return 0;
#endif
}

void dongle_storage_get_info(dongle_storage *sto)
{
    log_info("Getting flash information...\r\n");
    st.num_pages = 0;
#ifdef DONGLE_PLATFORM__ZEPHYR
    st.min_block_size = flash_get_write_block_size(st.dev);
    flash_page_foreach(st.dev, _flash_page_info_, sto);
#else
    DONGLE_NO_OP;
#endif
}

void dongle_storage_init_device(dongle_storage *sto)
{
#ifdef DONGLE_PLATFORM__ZEPHYR
    st.dev = device_get_binding(DT_CHOSEN_ZEPHYR_FLASH_CONTROLLER_LABEL);
#else
    DONGLE_NO_OP;
#endif
}

void dongle_storage_init(dongle_storage *sto)
{
    log_info("Initializing storage...\r\n");
    off = 0;
    st.map.enctr_entries = 0;
    dongle_storage_init_device(sto);
    dongle_storage_get_info(sto);
    log_infof("Pages: %d, Page Size: %u\r\n", st.num_pages, st.page_size);
    st.map.config = FLASH_OFFSET;
}

#define cf (*cfg)

void dongle_storage_load_config(dongle_storage *sto, dongle_config_t *cfg)
{
    log_info("Loading config...\r\n");
    off = st.map.config;
#define read(size, dst) (_flash_read_(sto, dst, size), off += size)
    read(sizeof(dongle_id_t), &cf.id);
    read(sizeof(dongle_timer_t), &cf.t_init);
    read(sizeof(key_size_t), &cf.backend_pk_size);
    read(PK_MAX_SIZE, &cf.backend_pk);
    read(sizeof(key_size_t), &cf.dongle_sk_size);
    read(SK_MAX_SIZE, &cf.dongle_sk);
    st.map.otp = off;
    // log start is pushed onto the next blank page
    st.map.log = next_multiple(st.page_size,
                               st.map.otp + (NUM_OTP * sizeof(dongle_otp_t)));
#undef read
}

void dongle_storage_save_config(dongle_storage *sto, dongle_config_t *cfg)
{
    log_debug("Saving config\r\n");
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
    log_debug("Saving OTPs\r\n");
    for (int i = 0; i < NUM_OTP; i++)
    {
        off = OTP(i);
        pre_erase(sto, sizeof(dongle_otp_t));
        _flash_write_(sto, &otps[i], sizeof(dongle_otp_t));
    }
}

int otp_is_used(dongle_otp_t *otp)
{
    return !((otp->flags & 0x0000000000000001) >> 0);
}

int dongle_storage_match_otp(dongle_storage *sto, uint64_t val)
{
    dongle_otp_t otp;
    for (int i = 0; i < NUM_OTP; i++)
    {
        dongle_storage_load_otp(sto, i, &otp);
        if (otp.val == val && !otp_is_used(&otp))
        {
            otp.flags &= 0xfffffffffffffffe;
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
#define ENCOUNTER_ENTRY_SIZE (sizeof(beacon_location_id_t) + sizeof(beacon_id_t) + \
                              sizeof(beacon_timer_t) + sizeof(dongle_timer_t) +    \
                              BEACON_EPH_ID_SIZE)

//#define ENCOUNTER_LOG_BASE (ENCOUNTER_BASE + sizeof(flash_check_t))
#define ENCOUNTER_LOG_OFFSET(j) (st.map.log + (j * ENCOUNTER_ENTRY_SIZE))

void dongle_storage_load_encounter(dongle_storage *sto,
                                   enctr_entry_counter_t i, dongle_encounter_cb cb)
{
    dongle_encounter_entry en;
    if (i >= st.map.enctr_entries)
    {
        log_errorf("Starting index for encounter log (%llu) is too large\r\n", i);
    }
    log_debugf("loading log entries starting at index %llu\r\n", i);
    do
    {
        if (i < st.map.enctr_entries)
        {
            dongle_storage_load_single_encounter(sto, i, &en);
        }
        else
        {
            break;
        }
        i++;
    } while (cb(i - 1, &en));
}

void dongle_storage_load_single_encounter(dongle_storage *sto,
                                          enctr_entry_counter_t i, dongle_encounter_entry *en)
{
    off = ENCOUNTER_LOG_OFFSET(i);
#define read(size, dst) _flash_read_(sto, dst, size), off += size
    read(sizeof(beacon_id_t), &en->beacon_id);
    read(sizeof(beacon_location_id_t), &en->location_id);
    read(sizeof(beacon_timer_t), &en->beacon_time);
    read(sizeof(dongle_timer_t), &en->dongle_time);
    read(BEACON_EPH_ID_SIZE, &en->eph_id);
#undef read
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
    log_debugf("write log; existing entries: %llu, offset: 0x%x\r\n", st.map.enctr_entries, off);
    pre_erase(sto, ENCOUNTER_ENTRY_SIZE);
#define write(data, size) \
    _flash_write_(sto, data, size), off += size
    write(beacon_id, sizeof(beacon_id_t));
    write(location_id, sizeof(beacon_location_id_t));
    write(beacon_time, sizeof(beacon_timer_t));
    write(dongle_time, sizeof(dongle_timer_t));
    write(eph_id, BEACON_EPH_ID_SIZE);
    log_debugf("total size: %u (entry size=%d)\r\n", off - start, ENCOUNTER_ENTRY_SIZE);
#undef write
    st.map.enctr_entries++;
    log_debugf("log now contains %llu entries\r\n", st.map.enctr_entries);
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