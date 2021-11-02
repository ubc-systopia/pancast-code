#define LOG_LEVEL__INFO

#include "storage.h"

#include <string.h>

#include "common/src/platform/gecko.h"
#include "common/src/util/log.h"
#include "common/src/util/util.h"

#define prev_multiple(k, n) ((n) - ((n) % (k)))
#define next_multiple(k, n) ((n) + ((k) - ((n) % (k)))) // buggy

#ifdef BEACON_PLATFORM__ZEPHYR
static bool _flash_page_info_(const struct flash_pages_info *info, void *data)
{
#define sto ((beacon_storage *)data)
  if (!sto->num_pages) {
      sto->page_size = info->size;
  } else if (info->size != sto->page_size) {
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
  log_debugf("%s", "erased.\r\n");
  st.numErasures++;
}

#define erase(addr) beacon_storage_erase(sto, addr)

void pre_erase(beacon_storage *sto, size_t write_size)
{
  // Erase before write
#define page_num(o) ((o) / st.page_size)
  if ((st.off % st.page_size) == 0) {
    erase(st.off);
  } else if (page_num(st.off + write_size) > page_num(st.off)) {
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
#ifdef VERBOSE_DEBUG_LOGGING
  log_debugf("writing %d bytes to flash at address 0x%x\r\n", size, st.off);
#endif
#ifdef BEACON_PLATFORM__ZEPHYR
  return flash_write(st.dev, st.off, data, size)
         ? log_errorf("%s", "Error writing flash\r\n"),
         1 : 0;
#else
  return MSC_WriteWord((uint32_t *)st.off, data, (uint32_t)size);
#endif
}

void beacon_storage_get_info(beacon_storage *sto)
{
#ifdef BEACON_PLATFORM__ZEPHYR
  log_infof("%s", "Getting flash information...\r\n");
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
  log_debugf("%s", "Initializing storage...\r\n");
  st.off = 0;
  st.numErasures = 0;
  beacon_storage_init_device(sto);
  beacon_storage_get_info(sto);
  log_infof("Pages: %d, Page Size: %u\r\n", st.num_pages, st.page_size);
  st.map.config = FLASH_OFFSET;
  if (FLASH_OFFSET % st.page_size != 0) {
    log_errorf("Storage area start addr %u is not page (%u) aligned!\r\n",
        FLASH_OFFSET, st.page_size);
  }
  st.map.stat = st.total_size - (3*st.page_size);
}

// Read data from flashed storage
// Format matches the fixed structure which is also used as a protocol when appending non-app
// data to the device image.
void beacon_storage_load_config(beacon_storage *sto, beacon_config_t *cfg)
{
  log_debugf("%s", "Loading config...\r\n");
  st.off = st.map.config;
#define read(size, dst) (_flash_read_(sto, dst, size), st.off += size)
  read(sizeof(beacon_id_t), &cfg->beacon_id);
  read(sizeof(beacon_location_id_t), &cfg->beacon_location_id);
  read(sizeof(beacon_timer_t), &cfg->t_init);
  read(sizeof(key_size_t), &cfg->backend_pk_size);
  if (cfg->backend_pk_size > PK_MAX_SIZE) {
    log_errorf("Key size read for backend pubkey (%u > %u)\r\n",
        cfg->backend_pk_size, PK_MAX_SIZE);
    cfg->backend_pk_size = PK_MAX_SIZE;
  }
  read(cfg->backend_pk_size, &cfg->backend_pk);
  read(sizeof(key_size_t), &cfg->beacon_sk_size);
  if (cfg->beacon_sk_size > SK_MAX_SIZE) {
    log_errorf("Key size read for beacon privkey (%u > %u)\r\n",
        cfg->beacon_sk_size, SK_MAX_SIZE);
    cfg->beacon_sk_size = SK_MAX_SIZE;
  }
  read(cfg->beacon_sk_size, &cfg->beacon_sk);
  read(sizeof(test_filter_size_t), &st.test_filter_size);
  if (st.test_filter_size != TEST_FILTER_LEN) {
      log_errorf("Warning: test filter length mismatch (%u != %u)\r\n", st.test_filter_size, TEST_FILTER_LEN);
      st.test_filter_size = TEST_FILTER_LEN;
  }
  st.map.test_filter = st.off;
#undef read
  log_debugf("%s", "Config loaded.\r\n");
  log_infof("    Flash offset:        %u\r\n", st.map.config);
  log_infof("    Test filter offset:  %u\r\n", st.map.test_filter);
  log_infof("    Stat offset:         %u\r\n", st.map.stat);
}

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

void beacon_storage_read_test_filter(beacon_storage *sto, uint8_t *buf)
{
  st.off = st.map.test_filter;
  _flash_read_(sto, buf, st.test_filter_size);
}

#undef block_align
#undef st
#undef align
#undef next_multiple
#undef prev_multiple
