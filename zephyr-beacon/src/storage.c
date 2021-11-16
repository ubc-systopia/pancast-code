#include "storage.h"

#include <string.h>

#define LOG_LEVEL__INFO

#include <include/log.h>
#include <include/util.h>

#define prev_multiple(k, n) ((n) - ((n) % (k)))
#define next_multiple(k, n) ((n) + ((k) - ((n) % (k)))) // buggy

static bool _flash_page_info_(const struct flash_pages_info *info, void *data)
{
#define sto ((beacon_storage *) data)
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

void beacon_storage_erase(beacon_storage *sto, storage_addr_t offset)
{
  log_debugf("erasing page at 0x%x\r\n", (offset));
  flash_erase(sto->dev, (offset), sto->page_size);
  log_debugf("%s", "erased.\r\n");
  sto->numErasures++;
}

void pre_erase(beacon_storage *sto, storage_addr_t off, size_t write_size)
{
  // Erase before write
#define page_num(o) ((o) / sto->page_size)
  if ((off % sto->page_size) == 0) {
    beacon_storage_erase(sto, off);
  } else if (page_num(off + write_size) > page_num(off)) {
#undef page_num
    beacon_storage_erase(sto, next_multiple(sto->page_size, off));
  }
}

int _flash_read_(beacon_storage *sto, storage_addr_t off, void *data, size_t size)
{
  log_debugf("size: %d bytes, addr: 0x%x, flash off: 0x%0x\r\n",
      size, off, sto->map.config);
  return flash_read(sto->dev, off, data, size);
}

int _flash_write_(beacon_storage *sto, storage_addr_t off, void *data, size_t size)
{
  int ret = flash_write(sto->dev, off, data, size);
  log_debugf("size: %d bytes, addr: 0x%x, flash off: 0x%0x, ret: %d\r\n",
      size, off, sto->map.config, ret);
  return ret;
}

void beacon_storage_get_info(beacon_storage *sto)
{
  sto->num_pages = 0;
  sto->min_block_size = flash_get_write_block_size(sto->dev);
  flash_page_foreach(sto->dev, _flash_page_info_, sto);
  sto->total_size = sto->num_pages * sto->page_size;
}

void beacon_storage_init_device(beacon_storage *sto)
{
  sto->dev = device_get_binding(DT_CHOSEN_ZEPHYR_FLASH_CONTROLLER_LABEL);
}

void beacon_storage_init(beacon_storage *sto)
{
  log_debugf("%s", "Initializing storage...\r\n");
  sto->numErasures = 0;
  beacon_storage_init_device(sto);
  beacon_storage_get_info(sto);
  log_infof("Pages: %d, Page Size: %u\r\n", sto->num_pages, sto->page_size);
  sto->map.config = FLASH_OFFSET;
  if (FLASH_OFFSET % sto->page_size != 0) {
    log_errorf("Storage area start addr %u is not page (%u) aligned!\r\n",
        FLASH_OFFSET, sto->page_size);
  }
//  sto->map.stat = sto->total_size - (3*sto->page_size);
}

// Read data from flashed storage
// Format matches the fixed structure which is also used as a protocol when appending non-app
// data to the device image.
void beacon_storage_load_config(beacon_storage *sto, beacon_config_t *cfg)
{
  log_debugf("%s", "Loading config...\r\n");
  storage_addr_t off = sto->map.config;
#define read(size, dst) (_flash_read_(sto, off, dst, size), off += size)
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
  read(sizeof(test_filter_size_t), &sto->test_filter_size);
  if (sto->test_filter_size != TEST_FILTER_LEN) {
      log_errorf("Test filter length mismatch (%u != %u)\r\n",
          sto->test_filter_size, TEST_FILTER_LEN);
      sto->test_filter_size = TEST_FILTER_LEN;
  }
  sto->map.test_filter = off;
  /*
   * important to place stats on a separate page that can be repeatedly
   * erased and written to in order to ensure persistence of stats correctly.
   */
  sto->map.stat = next_multiple(sto->page_size,
      sto->map.test_filter + sto->test_filter_size);
#undef read
  log_debugf("%s", "Config loaded.\r\n");
  log_infof("    Flash offset:        %u\r\n", sto->map.config);
  log_infof("    Test filter offset:  %u\r\n", sto->map.test_filter);
  log_infof("    Stat offset:         %u\r\n", sto->map.stat);
}

void beacon_storage_save_stat(beacon_storage *sto, void *stat, size_t len)
{
  beacon_storage_erase(sto, sto->map.stat);
  storage_addr_t off = sto->map.stat;
  _flash_write_(sto, off, stat, len);
}

void beacon_storage_read_stat(beacon_storage *sto, void *stat, size_t len)
{
  _flash_read_(sto, sto->map.stat, stat, len);
}

#undef next_multiple
#undef prev_multiple
