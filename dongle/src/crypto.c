#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "crypto.h"
#include "dongle.h"
#include "common/src/util/log.h"

static size_t test_offset;
int ctr_drbg_self_test_entropy( void *data, unsigned char *buf,
                                       size_t len )
{
    const unsigned char *p = data;
    memcpy( buf, p + test_offset, len );
    test_offset += len;
    return( 0 );
}


void run_mbedtls_benchmark(void)
{
#if 0
  float startts = 0.0, endts = 0.0;
  uint64_t starttick = 0, endtick = 0;

  int mt_ret2 = 0, mt_cnt = 20;
  char *rand256 = malloc(1024);
  memset(rand256, 0, 1024);

  static const unsigned char entropy_source_nopr[] =
      { 0x4c, 0xfb, 0x21, 0x86, 0x73, 0x34, 0x6d, 0x9d,
        0x50, 0xc9, 0x22, 0xe4, 0x9b, 0x0d, 0xfc, 0xd0,
        0x90, 0xad, 0xf0, 0x4f, 0x5c, 0x3b, 0xa4, 0x73,
        0x27, 0xdf, 0xcd, 0x6f, 0xa6, 0x3a, 0x78, 0x5c,
        0x01, 0x69, 0x62, 0xa7, 0xfd, 0x27, 0x87, 0xa2,
        0x4b, 0xf6, 0xbe, 0x47, 0xef, 0x37, 0x83, 0xf1,
        0xb7, 0xec, 0x46, 0x07, 0x23, 0x63, 0x83, 0x4a,
        0x1b, 0x01, 0x33, 0xf2, 0xc2, 0x38, 0x91, 0xdb,
        0x4f, 0x11, 0xa6, 0x86, 0x51, 0xf2, 0x3e, 0x3a,
        0x8b, 0x1f, 0xdc, 0x03, 0xb1, 0x92, 0xc7, 0xe7 };

  static const unsigned char pers_nopr[] =
      { 0x88, 0xee, 0xb8, 0xe0, 0xe8, 0x3b, 0xf3, 0x29,
        0x4b, 0xda, 0xcd, 0x60, 0x99, 0xeb, 0xe4, 0xbf,
        0x55, 0xec, 0xd9, 0x11, 0x3f, 0x71, 0xe5, 0xeb,
        0xcb, 0x45, 0x75, 0xf3, 0xd6, 0xa6, 0x8a, 0x6b };


  struct mbedtls_ctr_drbg_context mtc;
  mbedtls_ctr_drbg_init(&mtc);
  mbedtls_ctr_drbg_set_entropy_len( &mtc, MBEDTLS_CTR_DRBG_KEYSIZE);
  mbedtls_ctr_drbg_set_nonce_len( &mtc, MBEDTLS_CTR_DRBG_KEYSIZE / 2 );
  mbedtls_ctr_drbg_seed(&mtc,
                        ctr_drbg_self_test_entropy,
                        (void *) entropy_source_nopr,
                        pers_nopr, MBEDTLS_CTR_DRBG_KEYSIZE);
//  mbedtls_ctr_drbg_set_prediction_resistance( &mtc, MBEDTLS_CTR_DRBG_PR_ON );
  mbedtls_ctr_drbg_reseed( &mtc, NULL, 0 );
//  int mt_ret = mbedtls_ctr_drbg_self_test(0);
//  log_infof("mt_ret: %d random init\r\n", mt_ret);

  float ts32 = 0.0, sum_ts32 = 0.0, max_ts32 = 0.0, min_ts32 = 999.9;
  uint32_t tick32 = 0, sum_tick32 = 0, min_tick32 = 999, max_tick32 = 0;
  uint32_t rand32 = 0;
  for (int tmp = 0; tmp < mt_cnt; tmp++) {
    starttick = sl_sleeptimer_get_tick_count64();
    startts = now();
    mt_ret2 = mbedtls_ctr_drbg_random(&mtc, (unsigned char *) &rand32, sizeof(uint32_t));
    endts = now();
    endtick = sl_sleeptimer_get_tick_count64();
    ts32 = endts - startts;
    tick32 = endtick - starttick;
    if (max_ts32 < ts32)
      max_ts32 = ts32;
    if (min_ts32 > ts32)
      min_ts32 = ts32;

    if (max_tick32 < tick32)
      max_tick32 = tick32;
    if (min_tick32 > tick32)
      min_tick32 = tick32;

    sum_ts32 += ts32;
    sum_tick32 += tick32;
    log_infof("sz: %u, val: %u, time: %f ms, tick: %u\r\n", sizeof(uint32_t), rand32, ts32, tick32);
  }
  log_infof("random time avg: %f ms, min: %f ms, max: %f ms\r\n",
    (sum_ts32/mt_cnt), min_ts32, max_ts32);

  log_infof("random tick avg: %f, min: %u ms, max: %u ms\r\n",
    ((float) sum_tick32/mt_cnt), min_tick32, max_tick32);

  startts = 0.0; endts = 0.0;
  float ts256 = 0.0, sum_ts256 = 0.0, max_ts256 = 0.0, min_ts256 = 999.9;
  starttick = 0, endtick = 0;
  uint32_t tick256 = 0, sum_tick256 = 0, min_tick256 = 999, max_tick256 = 0;
  mt_ret2 += 1; mt_cnt = 20;
  for (int tmp = 0; tmp < mt_cnt; tmp++) {
    starttick = sl_sleeptimer_get_tick_count64();
    startts = now();
    mt_ret2 = mbedtls_ctr_drbg_random(&mtc, (unsigned char *) rand256, 1024);
    endts = now();
    endtick = sl_sleeptimer_get_tick_count64();
    ts256 = endts - startts;
    tick256 = endtick - starttick;
    if (max_ts256 < ts256)
      max_ts256 = ts256;
    if (min_ts256 > ts256)
      min_ts256 = ts256;

    if (max_tick256 < tick256)
      max_tick256 = tick256;
    if (min_tick256 > tick256)
      min_tick256 = tick256;

    sum_ts256 += ts256;
    sum_tick256 += tick256;
    log_infof("sz: %u, v[0]: %u, v[1]: %u, time: %f ms, tick: %u\r\n",
      1024, ((uint32_t *) rand256)[0], ((uint32_t *) rand256)[1], ts256, tick256);
  }

  log_infof("random time avg: %f ms, min: %f ms, max: %f ms\r\n",
    (sum_ts256/mt_cnt), min_ts256, max_ts256);

  log_infof("random tick avg: %f, min: %u ms, max: %u ms\r\n",
    ((float) sum_tick256/mt_cnt), min_tick256, max_tick256);

  // ========

  free(rand256);
#endif
}

void run_psa_benchmark(void)
{
  float startts = 0.0, endts = 0.0;
  uint64_t starttick = 0, endtick = 0;
  int mt_ret2 = 0, mt_cnt = 20;
  float ts256 = 0.0, sum_ts256 = 0.0, max_ts256 = 0.0, min_ts256 = 999.9;
  starttick = 0, endtick = 0;
  uint32_t tick256 = 0, sum_tick256 = 0, min_tick256 = 999, max_tick256 = 0;
  char *rand256 = malloc(1024);
  memset(rand256, 0, 1024);

  log_infof("Init PSA crypto ret: %d\r\n", mt_ret2);

  mt_ret2 = psa_crypto_init();
  startts = 0.0; endts = 0.0;
  ts256 = 0.0, sum_ts256 = 0.0, max_ts256 = 0.0, min_ts256 = 999.9;
  starttick = 0, endtick = 0;
  tick256 = 0, sum_tick256 = 0, min_tick256 = 999, max_tick256 = 0;
  mt_ret2 += 1; mt_cnt = 20;

  for (int tmp = 0; tmp < mt_cnt; tmp++) {
    starttick = sl_sleeptimer_get_tick_count64();
    startts = now();
    mt_ret2 = psa_generate_random((uint8_t *) rand256, 1024);
    endts = now();
    endtick = sl_sleeptimer_get_tick_count64();
    ts256 = endts - startts;
    tick256 = endtick - starttick;
    if (max_ts256 < ts256)
      max_ts256 = ts256;
    if (min_ts256 > ts256)
      min_ts256 = ts256;

    if (max_tick256 < tick256)
      max_tick256 = tick256;
    if (min_tick256 > tick256)
      min_tick256 = tick256;

    sum_ts256 += ts256;
    sum_tick256 += tick256;
    log_infof("sz: %u, v[0]: %u, v[1]: %u, time: %f ms, tick: %u\r\n",
      1024, ((uint32_t *) rand256)[0], ((uint32_t *) rand256)[1], ts256, tick256);
  }

  log_infof("random time avg: %f ms, min: %f ms, max: %f ms\r\n",
    (sum_ts256/mt_cnt), min_ts256, max_ts256);

  log_infof("random tick avg: %f, min: %u ms, max: %u ms\r\n",
    ((float) sum_tick256/mt_cnt), min_tick256, max_tick256);

  mbedtls_psa_crypto_free();

  // ========

  free(rand256);
}
