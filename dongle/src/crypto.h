#ifndef __CRYPTO_H__
#define __CRYPTO_H__

#include "sx_trng.h"
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/psa_util.h>
// #include <mbedtls/sl_crypto/include/trng.h>

#define DONGLE_CRYPTO 0

extern void run_mbedtls_benchmark(void);
extern void run_psa_benchmark(void);

#endif /* __CRYPTO_H__ */
