#ifndef COMMON_LOG__H
#define COMMON_LOG__H

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
// log_error, log_errorf
// log_info, log_infof
// log_debug, log_debugf
// log_telem, log_telemf

#define TAG_DEBUG "DEBUG"
#define TAG_INFO "INFO"
#define TAG_ERROR "ERROR"
#define TAG_TELEM "TELEM"

#define LOG_SHOW_TAGS

#define LOG_NA NULL

// // If telemetry is enabled, show tags always
#ifdef LOG_LEVEL__TELEM
#ifndef LOG_SHOW_TAGS
#define LOG_SHOW_TAGS
#endif
#endif
#ifdef LOG_SHOW_TAGS
#define logf(tag, fmtstr, args...) (printk("[%s] ", tag), printk(fmtstr, args))
#else
#define logf(tag, fmtstr, args...) printk(fmtstr, args)
#endif

#define log(tag, str) logf(tag, "%s", str)

#ifdef LOG_LEVEL__ERROR
#define log_error(s) log(TAG_ERROR, s)
#define log_errorf(f, a...) logf(TAG_ERROR, f, a)
#define LOG_LEVEL TAG_ERROR
#endif

#ifdef LOG_LEVEL__INFO
#define log_error(s) log(TAG_ERROR, s)
#define log_errorf(f, a...) logf(TAG_ERROR, f, a)
#define log_info(s) log(TAG_INFO, s)
#define log_infof(f, a...) logf(TAG_INFO, f, a)
#define LOG_LEVEL TAG_INFO
#endif

#ifdef LOG_LEVEL__DEBUG
#define log_error(s) log(TAG_ERROR, s)
#define log_errorf(f, a...) logf(TAG_ERROR, f, a)
#define log_info(s) log(TAG_INFO, s)
#define log_infof(f, a...) logf(TAG_INFO, f, a)
#define log_debug(s) log(TAG_DEBUG, s)
#define log_debugf(f, a...) logf(TAG_DEBUG, f, a)
#define LOG_LEVEL TAG_DEBUG
#endif

#ifdef LOG_LEVEL__TELEM
#define log_error(s) log(TAG_ERROR, s)
#define log_errorf(f, a...) logf(TAG_ERROR, f, a)
#define log_info(s) log(TAG_INFO, s)
#define log_infof(f, a...) logf(TAG_INFO, f, a)
#define log_telem(s) log(TAG_TELEM, s)
#define log_telemf(f, a...) logf(TAG_TELEM, f, a)
#define LOG_LEVEL TAG_TELEM
#endif

#ifndef LOG_LEVEL
#warning "No log level set"
#define LOG_LEVEL "NONE"
#endif

#ifndef log_error
#define log_error(_) LOG_NA
#define log_errorf(_...) LOG_NA
#endif

#ifndef log_info
#define log_info(_) LOG_NA
#define log_infof(_...) LOG_NA
#endif

#ifndef log_debug
#define log_debug(_) LOG_NA
#define log_debugf(_...) LOG_NA
#endif

#ifndef log_telem
#define log_telem(_) LOG_NA
#define log_telemf(_...) LOG_NA
#endif

#endif