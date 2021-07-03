#ifndef DONGLE_MUTEX__H
#define DONGLE_MUTEX__H

// Implementation of mutual-exclusion functions based on the standard
// atomicity library. Inspired by
// https://gist.github.com/mepcotterell/8df8e9c742fa6f926c39667398846048

#include <stdatomic.h>

typedef atomic_flag mutex;

void mutex_lock(mutex *);
void mutex_unlock(mutex *);

#endif