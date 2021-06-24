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

#define LOG_NA NULL

#define log(str) printk(str)
#define logf(fmtstr, args...) printk(fmtstr, args)

#ifdef LOG_LEVEL__ERROR
#define log_error log
#define log_errorf logf
#define log_info(_) LOG_NA
#define log_infof(_...) LOG_NA
#define log_debug(_) LOG_NA
#define log_debugf(_...) LOG_NA
#define LOG_LEVEL
#endif

#ifdef LOG_LEVEL__INFO
#define log_error log
#define log_errorf logf
#define log_info log
#define log_infof logf
#define log_debug(_) LOG_NA
#define log_debugf(_...) LOG_NA
#define LOG_LEVEL
#endif

#ifdef LOG_LEVEL__DEBUG
#define log_error log
#define log_errorf logf
#define log_info log
#define log_infof logf
#define log_debug log
#define log_debugf logf
#define LOG_LEVEL
#endif

#ifndef LOG_LEVEL
#warning "No log level set"
#define log_error(_) LOG_NA
#define log_errorf(_...) LOG_NA
#define log_info(_) LOG_NA
#define log_infof(_...) LOG_NA
#define log_debug(_) LOG_NA
#define log_debugf(_...) LOG_NA
#endif

#endif