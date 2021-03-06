/***************************************************************************//**
 * @file
 * @brief iostream usart examples functions
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
#include <unistd.h>
#include <inttypes.h>
#include "em_chip.h"
#include "sl_iostream.h"
#include "sl_iostream_init_instances.h"
#include "sl_iostream_handles.h"

/*******************************************************************************
 *******************************   DEFINES   ***********************************
 ******************************************************************************/

#ifndef BUFSIZE
#define BUFSIZE    4096
#endif

/*******************************************************************************
 ***************************  LOCAL VARIABLES   ********************************
 ******************************************************************************/

/* Input buffer */
//static char buffer[BUFSIZE];

/* Data array */
uint8_t risk_data_buffer[4096];
int data_ready = 0;
int data_len = 0;



/*******************************************************************************
 **************************   GLOBAL FUNCTIONS   *******************************
 ******************************************************************************/


/***************************************************************************//**
 * Initialize example.
 ******************************************************************************/
void app_iostream_eusart_init(void)
{
  /* Prevent buffering of output/input.*/
#if !defined(__CROSSWORKS_ARM) && defined(__GNUC__)
  setvbuf(stdout, NULL, _IONBF, 0);   /*Set unbuffered mode for stdout (newlib)*/
  setvbuf(stdin, NULL, _IONBF, 0);   /*Set unbuffered mode for stdin (newlib)*/
#endif

  sl_iostream_set_default(sl_iostream_vcom_handle);

}

/***************************************************************************//**
 * Example ticking function.
 ******************************************************************************/
void app_iostream_eusart_process_action(void)
{
  int c = 0;
  static uint32_t index = 0;

  c = getchar();
  if (c > 0) {
    if (c == '\n' || c == '\r') {
      printf("\r\nrisk buffer: %s\r\n",risk_data_buffer);
      data_len = index;
      data_ready = 1;
      index = 0;
    } else {
      if (index < BUFSIZE - 1) {
        risk_data_buffer[index] = (uint8_t)c;
        index++;
      }
      /* Local echo */
      putchar(c);
    }
  }
}
