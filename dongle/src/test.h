#ifndef DONGLE_TEST__H
#define DONGLE_TEST__H

#include "dongle.h"
#include "storage.h"

#if TEST_DONGLE
static otp_set TEST_OTPS = {
  {0xffffffffffffffff,
   11111111},
  {0xffffffffffffffff,
   11112222},
  {0xffffffffffffffff,
   12121212},
  {0xffffffffffffffff,
   11010101},
  {0xffffffffffffffff,
   11144111},
  {0xffffffffffffffff,
   11111111},
  {0xffffffffffffffff,
   11115611},
  {0xffffffffffffffff,
   11111118},
  {0xffffffffffffffff,
   11111117},
  {0xffffffffffffffff,
   11111116},
  {0xffffffffffffffff,
   11118911},
  {0xffffffffffffffff,
   11111122},
  {0xffffffffffffffff,
   11334111},
  {0xffffffffffffffff,
   11111111},
  {0xffffffffffffffff,
   11111111},
  {0xffffffffffffffff,
   12345678}
};

void dongle_test();
void dongle_test_encounter(encounter_broadcast_t *enc);
#endif /* TEST_DONGLE */

#endif
