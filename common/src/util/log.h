#ifndef COMMON_LOG__H
#define COMMON_LOG__H

#include <stdio.h>
#include <assert.h>
#define LOG_NA assert(1)

#ifdef BEACON_PLATFORM__ZEPHYR

// Simple logging Macros provided mostly as placeholders for a
// more robust logging mechanism, and to differentiate between
// the types of outputs.
// Log level depends on a per-file definition of LOG_LEVEL__X
// Which allows for static toggling of log-verbosity on a
// relatively per-feature basis
// If the definition is not present, compilation warnings will
// appear and logging statements will be no-ops.
//
// Levels:
// LOG_LEVEL__NONE
// LOG_LEVEL__ERROR
// LOG_LEVEL__INFO
// LOG_LEVEL__DEBUG
// LOG_LEVEL__TELEM
//
// Provided macros:
// log_errorf
// log_infof
// log_debugf
// log_telemf

#define TAG_DEBUG "DEBUG"
#define TAG_INFO "INFO"
#define TAG_ERROR "ERROR"
#define TAG_TELEM "TELEM"

// // If telemetry is enabled, show tags always
#ifdef LOG_LEVEL__TELEM
#ifndef LOG_SHOW_TAGS
#define LOG_SHOW_TAGS
#endif
#endif
#ifdef LOG_SHOW_TAGS
#define logf(tag, fmtstr, args...) (printf("[%s] ", tag), printf(fmtstr, args))
#else
#define logf(tag, fmtstr, args...) printf(fmtstr, args)
#endif

#define log(tag, str) logf(tag, "%s", str)

#ifdef LOG_LEVEL__ERROR
#define log_errorf(f, a...) logf(TAG_ERROR, f, a)
#define LOG_LEVEL TAG_ERROR
#endif

#ifdef LOG_LEVEL__INFO
#define log_errorf(f, a...) logf(TAG_ERROR, f, a)
#define log_infof(f, a...) logf(TAG_INFO, f, a)
#define LOG_LEVEL TAG_INFO
#endif

#ifdef LOG_LEVEL__DEBUG
#define log_errorf(f, a...) logf(TAG_ERROR, f, a)
#define log_infof(f, a...) logf(TAG_INFO, f, a)
#define log_debugf(f, a...) logf(TAG_DEBUG, f, a)
#define LOG_LEVEL TAG_DEBUG
#endif

#ifdef LOG_LEVEL__TELEM
#define log_errorf(f, a...) logf(TAG_ERROR, f, a)
#define log_infof(f, a...) logf(TAG_INFO, f, a)
#define log_telemf(f, a...) logf(TAG_TELEM, f, a)
#define LOG_LEVEL TAG_TELEM
#endif

#ifndef LOG_LEVEL
#warning "No log level set"
#define LOG_LEVEL "NONE"
#endif

#ifndef log_errorf
#define log_errorf(_...) LOG_NA
#endif

#ifndef log_infof
#define log_infof(_...) LOG_NA
#endif

#ifndef log_debugf
#define log_debugf(_...) LOG_NA
#endif

#ifndef log_telemf
#define log_telemf(_...) LOG_NA
#endif

#else /* BEACON_PLATFORM__GECKO */

#include "app_log.h"

#define log_errorf app_log_critical

#define log_infof app_log_info

#define log_debugf app_log_debug

#define log_telem_tag printf("TELEM ")
#define log_telemf(fmtstr, args...) LOG_NA


#endif /* PLATFORM CONFIG */
#endif /* COMMON_LOG_H */