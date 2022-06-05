#ifndef COMMON_LOG__H
#define COMMON_LOG__H

#include <stdio.h>
#include <assert.h>

#define LOG_LVL LVL_EXP

#define LVL_EXP 0
#define LVL_DBG 1

#if LOG_LVL == LVL_DBG
#define log_expf(fmtstr, args...)  printf("%s:%d "fmtstr, __func__, __LINE__, args)
#define log_errorf(fmtstr, args...)  printf("%s:%d "fmtstr, __func__, __LINE__, args)
#define log_infof(fmtstr, args...) printf("%s:%d "fmtstr, __func__, __LINE__, args)
#define log_debugf(fmtstr, args...) printf("%s:%d "fmtstr, __func__, __LINE__, args)
#else /* LOG_LVL == LVL_EXP */
#define log_expf(fmtstr, args...)  printf("%s:%d "fmtstr, __func__, __LINE__, args)
#define log_errorf(fmtstr, args...)  printf("%s:%d "fmtstr, __func__, __LINE__, args)
#define log_infof(fmtstr, args...) do {} while (0)
#define log_debugf(fmtstr, args...) do {} while (0)

#endif /* LOG_LVL */

#endif /* COMMON_LOG_H */
