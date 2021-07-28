#define LOG_LEVEL__INFO

#include "storage.h"

#include <string.h>

#include "../../common/src/gecko.h"
#include "../../common/src/log.h"
#include "../../common/src/util.h"

#define prev_multiple(k, n) ((n) - ((n) % (k)))
#define next_multiple(k, n) ((n) + ((k) - ((n) % (k)))) // buggy

#ifdef BEACON_PLATFORM__ZEPHYR
static bool _flash_page_info_(const struct flash_pages_info *info, void *data)
{
#define sto ((beacon_storage *)data)
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

void beacon_storage_erase(beacon_storage *sto, storage_addr_t offset)
{
    log_debugf("erasing page at 0x%x\r\n", (offset));
#ifdef BEACON_PLATFORM__ZEPHYR
    flash_erase(st.dev, (offset), st.page_size);
#else
    MSC_ErasePage((uint32_t *)offset);
#endif
    log_debug("erased.\r\n");
    st.numErasures++;
}

#define erase(addr) beacon_storage_erase(sto, addr)

void pre_erase(beacon_storage *sto, size_t write_size)
{
// Erase before write
#define page_num(o) ((o) / st.page_size)
    if ((st.off % st.page_size) == 0)
    {
        erase(st.off);
    }
    else if (page_num(st.off + write_size) > page_num(st.off))
    {
#undef page_num
        erase(next_multiple(st.page_size, st.off));
    }
}

int _flash_read_(beacon_storage *sto, void *data, size_t size)
{
    log_debugf("reading %d bytes from flash at address 0x%x\r\n", size, st.off);
#ifdef BEACON_PLATFORM__ZEPHYR
    return flash_read(st.dev, st.off, data, size);
#else
    memcpy(data, (uint32_t *)st.off, size);
    return 0;
#endif
}

int _flash_write_(beacon_storage *sto, void *data, size_t size)
{
    log_debugf("writing %d bytes to flash at address 0x%x\r\n", size, st.off);
#ifdef BEACON_PLATFORM__ZEPHYR
    return flash_write(st.dev, st.off, data, size)
           ? log_error("Error writing flash\r\n"),
           1 : 0;
#else
    MSC_WriteWord((uint32_t *)st.off, data, (uint32_t)size);
    return 0;
#endif
}

void beacon_storage_get_info(beacon_storage *sto)
{
#ifdef BEACON_PLATFORM__ZEPHYR
    log_info("Getting flash information...\r\n");
    st.num_pages = 0;
    st.min_block_size = flash_get_write_block_size(st.dev);
    flash_page_foreach(st.dev, _flash_page_info_, sto);
#else
    st.num_pages = FLASH_DEVICE_NUM_PAGES;
    st.min_block_size = FLASH_DEVICE_BLOCK_SIZE;
    st.page_size = FLASH_DEVICE_PAGE_SIZE;
#endif
    st.total_size = st.num_pages * st.page_size;
}

void beacon_storage_init_device(beacon_storage *sto)
{
#ifdef BEACON_PLATFORM__ZEPHYR
    st.dev = device_get_binding(DT_CHOSEN_ZEPHYR_FLASH_CONTROLLER_LABEL);
#else
    MSC_ExecConfig_TypeDef execConfig = MSC_EXECCONFIG_DEFAULT;
    st.mscExecConfig = execConfig;
    MSC_ExecConfigSet(&st.mscExecConfig);
    MSC_Init();
#endif
}

void beacon_storage_init(beacon_storage *sto)
{
    log_info("Initializing storage...\r\n");
    st.off = 0;
    st.numErasures = 0;
    beacon_storage_init_device(sto);
    beacon_storage_get_info(sto);
    log_infof("Pages: %d, Page Size: %u\r\n", st.num_pages, st.page_size);
    st.map.config = FLASH_OFFSET;
    if (FLASH_OFFSET % st.page_size != 0)
    {
        log_error("Start address of storage area is not a page-multiple!\r\n");
    }
    st.map.stat = st.map.config + st.page_size;
}

#define cf (*cfg)

// Read data from flashed storage
// Format matches the fixed structure which is also used as a protocol when appending non-app
// data to the device image.
void beacon_storage_load_config(beacon_storage *sto, beacon_config_t *cfg)
{
    log_info("Loading config...\r\n");
    st.off = st.map.config;
#define read(size, dst) (_flash_read_(sto, dst, size), st.off += size)
    read(sizeof(beacon_id_t), &cf.beacon_id);
    read(8, &cf.beacon_location_id);
    read(sizeof(beacon_timer_t), &cf.t_init);
    read(sizeof(key_size_t), &cf.backend_pk_size);
    if (cf.backend_pk_size > PK_MAX_SIZE)
    {
        log_errorf("Key size read for public key (%u bytes) is larger than max (%u)\r\n",
                   cf.backend_pk_size, PK_MAX_SIZE);
    }
    read(cf.backend_pk_size, &cf.backend_pk);
    read(sizeof(key_size_t), &cf.beacon_sk_size);
    if (cf.beacon_sk_size > SK_MAX_SIZE)
    {
        log_errorf("Key size read for secret key (%u bytes) is larger than max (%u)\r\n",
                   cf.beacon_sk_size, SK_MAX_SIZE);
    }
    read(cf.beacon_sk_size, &cf.beacon_sk);
#undef read
    log_info("Config loaded.\r\n");
}

#undef cf

void beacon_storage_save_stat(beacon_storage *sto, void *stat, size_t len)
{
    beacon_storage_erase(sto, st.map.stat);
    st.off = st.map.stat;
    _flash_write_(sto, stat, len);
}

void beacon_storage_read_stat(beacon_storage *sto, void *stat, size_t len)
{
    st.off = st.map.stat;
    _flash_read_(sto, stat, len);
}

#undef block_align
#undef st
#undef align
#undef next_multiple
#undef prev_multiple
