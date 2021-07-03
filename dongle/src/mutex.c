#include "mutex.h"

void mutex_lock(mutex *mu)
{
    for (; atomic_flag_test_and_set(mu);)
        ;
}

void mutex_unlock(mutex *mu)
{
    atomic_flag_clear(mu);
}