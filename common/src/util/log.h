#ifndef __COMMON_LOG_H__
#define __COMMON_LOG_H__

#include <stdio.h>
#include <assert.h>
#include "app_log.h"

#define log_errorf app_log_critical

#define log_infof app_log_info

#define log_debugf app_log_debug

#endif /* __COMMON_LOG_H__ */
