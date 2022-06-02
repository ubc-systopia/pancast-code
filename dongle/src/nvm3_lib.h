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

#ifndef __NVM3_LIB_H__
#define __NVM3_LIB_H__

#include "dongle.h"
#include "storage.h"

/*******************************************************************************
 *******************************   DEFINES   ***********************************
 ******************************************************************************/

// Use the default nvm3 handle from nvm3_default.h
#define NVM3_DEFAULT_HANDLE nvm3_defaultHandle

/***************************************************************************//**
 * Initialize NVM3 example
 ******************************************************************************/
void nvm3_app_init(void);

size_t nvm3_count_objects(void);
size_t nvm3_get_erase_count(void);
void nvm3_save_config(dongle_storage *sto, dongle_config_t *cfg);
void nvm3_save_clock_cursor(dongle_storage *sto, dongle_config_t *cfg);
void nvm3_load_config(dongle_storage *sto, dongle_config_t *cfg);

/***************************************************************************//**
 * NVM3 ticking function
 ******************************************************************************/
void nvm3_app_process_action(void);

#endif  // __NVM3_LIB_H__
