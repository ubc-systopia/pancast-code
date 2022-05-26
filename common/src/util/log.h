#ifndef __COMMON_LOG_H__
#define __COMMON_LOG_H__

#include <stdio.h>
#include <assert.h>
#include "app_log.h"

#define LOG_LVL LVL_EXP

#define LVL_EXP 0
#define LVL_DBG 1

#if LOG_LVL == LVL_DBG
#define log_expf(...)  app_log_level(APP_LOG_LEVEL_CRITICAL, __VA_ARGS__)
#define log_errorf(...)  app_log_level(APP_LOG_LEVEL_CRITICAL, __VA_ARGS__)
#define log_infof(...) app_log_level(APP_LOG_LEVEL_INFO, __VA_ARGS__)
#define log_debugf(...) app_log_level(APP_LOG_LEVEL_DEBUG, __VA_ARGS__)
#else /* LOG_LVL == LVL_EXP */
#define log_expf(...)  app_log_level(APP_LOG_LEVEL_CRITICAL, __VA_ARGS__)
#define log_errorf(...)  app_log_level(APP_LOG_LEVEL_CRITICAL, __VA_ARGS__)
#define log_infof(...) do {} while (0)
#define log_debugf(...) do {} while (0)

#endif /* LOG_LVL */

#endif /* __COMMON_LOG_H__ */
