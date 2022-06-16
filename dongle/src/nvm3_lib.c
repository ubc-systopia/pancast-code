/***************************************************************************//**
 * @file
 * @brief NVM3 examples functions
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/
#include <stdio.h>
#include <string.h>

#include "nvm3_lib.h"
#include "nvm3_default.h"
#include "nvm3_default_config.h"
#include "common/src/util/log.h"

/*******************************************************************************
 **************************   LOCAL VARIABLES   ********************************
 ******************************************************************************/

enum {
  NVM3_CNT_T_CUR = 0,
  NVM3_CNT_LOG_HEAD,
  NVM3_CNT_LOG_TAIL,
  NVM3_MAX_COUNTERS
};

// Max and min keys for data objects
#define MIN_DATA_KEY  NVM3_KEY_MIN
#define MAX_DATA_KEY  (MIN_DATA_KEY + NVM3_MAX_COUNTERS - 1)

/*******************************************************************************
 **************************   GLOBAL FUNCTIONS   *******************************
 ******************************************************************************/

/***************************************************************************//**
 * Delete all data in NVM3.
 *
 * It deletes all data stored in NVM3.s
 ******************************************************************************/
void nvm3_app_reset(void *arguments)
{
  (void)&arguments;
//  printf("Deleting all data stored in NVM3\r\n");
  nvm3_eraseAll(NVM3_DEFAULT_HANDLE);
  // This deletes the counters, too, so they must be re-initialised
//  initialise_counters();
}

/***************************************************************************//**
 * Initialize NVM3 example.
 ******************************************************************************/
void nvm3_app_init(void)
{
  Ecode_t err;

  // This will call nvm3_open() with default parameters for
  // memory base address and size, cache size, etc.
  err = nvm3_initDefault();
  EFM_ASSERT(err == ECODE_NVM3_OK);
}

size_t nvm3_count_objects(void)
{
  nvm3_ObjectKey_t keys[NVM3_MAX_COUNTERS];
  memset(keys, 0, sizeof(nvm3_ObjectKey_t) * NVM3_MAX_COUNTERS);

  size_t nvm3_objcnt = nvm3_enumObjects(NVM3_DEFAULT_HANDLE, (uint32_t *) keys,
      sizeof(keys)/sizeof(keys[0]), MIN_DATA_KEY, MAX_DATA_KEY);

#if LOG_LVL == LVL_DBG
  size_t i, len;
  uint32_t type;
  for (i = 0; i < nvm3_cnt; i++) {
    nvm3_getObjectInfo(NVM3_DEFAULT_HANDLE, keys[i], &type, &len);
    log_expf("[NVM:%d] type: 0x%0x len: %u\r\n", i, type, len);
  }
#endif

  return nvm3_objcnt;

//  return nvm3_countObjects(NVM3_DEFAULT_HANDLE);
}

size_t nvm3_get_erase_count(void)
{
  uint32_t erasecnt = 0;
  Ecode_t err = nvm3_getEraseCount(NVM3_DEFAULT_HANDLE, &erasecnt);
  if (err != ECODE_NVM3_OK) {
    log_expf("cnt: %u err: 0x%0x\r\n", erasecnt, err);
    return 0;
  }

  return erasecnt;
}

void nvm3_save_config(dongle_config_t *cfg)
{
  Ecode_t err[NVM3_MAX_COUNTERS] __attribute__((unused));
  int i = 0;
#define nvm3_write(cntr_id, val)  \
  err[i++] = nvm3_writeCounter(NVM3_DEFAULT_HANDLE, cntr_id, val)

  nvm3_write(NVM3_CNT_T_CUR, cfg->t_cur);
  nvm3_write(NVM3_CNT_LOG_HEAD, cfg->en_head);
  nvm3_write(NVM3_CNT_LOG_TAIL, cfg->en_tail);

  log_infof("[NVM3] Tc: %u H: %u T: %u, errs: 0x%0x 0x%0x 0x%0x\r\n",
      cfg->t_cur, cfg->en_head, cfg->en_tail, err[0], err[1], err[2]);

#undef nvm3_write
}

void nvm3_save_clock_cursor(dongle_config_t *cfg)
{
  Ecode_t err[NVM3_MAX_COUNTERS] __attribute__((unused));
  int i = 0;
#define nvm3_write(cntr_id, val)  \
  err[i++] = nvm3_writeCounter(NVM3_DEFAULT_HANDLE, cntr_id, val)

  nvm3_write(NVM3_CNT_T_CUR, cfg->t_cur);
  nvm3_write(NVM3_CNT_LOG_HEAD, cfg->en_head);
  nvm3_write(NVM3_CNT_LOG_TAIL, cfg->en_tail);

  log_infof("[NVM3] Tc: %u H: %u T: %u, errs: 0x%0x 0x%0x 0x%0x\r\n",
      cfg->t_cur, cfg->en_head, cfg->en_tail, err[0], err[1], err[2]);

#undef nvm3_write
}

void nvm3_load_config(dongle_config_t *cfg)
{
  Ecode_t err[NVM3_MAX_COUNTERS];
  int i = 0;

#define nvm3_read(cntr_id, valp)  \
  err[i++] = nvm3_readCounter(NVM3_DEFAULT_HANDLE, cntr_id, valp)

  nvm3_read(NVM3_CNT_T_CUR, &cfg->t_cur);
  nvm3_read(NVM3_CNT_LOG_HEAD, &cfg->en_head);
  nvm3_read(NVM3_CNT_LOG_TAIL, &cfg->en_tail);

  log_expf("[NVM3] Tc: %u H: %u T: %u, errs: 0x%0x 0x%0x 0x%0x\r\n",
      cfg->t_cur, cfg->en_head, cfg->en_tail, err[0], err[1], err[2]);

#undef nvm3_read
}

/***************************************************************************//**
 * NVM3 ticking function.
 ******************************************************************************/
void nvm3_app_process_action(void)
{
  // Check if NVM3 controller can release any out-of-date objects
  // to free up memory.
  // This may take more than one call to nvm3_repack()
  while (nvm3_repackNeeded(NVM3_DEFAULT_HANDLE)) {
    log_infof("Repacking NVM...\r\n");
    nvm3_repack(NVM3_DEFAULT_HANDLE);
  }
}
