#ifndef COMMON_STAT__H
#define COMMON_STAT__H

#include <math.h>

typedef struct {
  double mu;
  double mu_0; // for computation
  double var; // variance
  double sigma;
  double n;
} stat_t;

// Update mean and (sample) variance in-place
// Variance incremental update is based on this derivation:
// https://math.stackexchange.com/a/103025
// which leverages an "orthogonality trick"
#define stat_add(val,stat)                                                \
  stat.mu_0 = stat.mu,                                                    \
  stat.mu = ((stat.mu * stat.n) + val) / (stat.n + 1),                    \
  stat.var =  stat.n > 0 ?                                                \
  ((((stat.n - 1) * stat.var)                                             \
    + (stat.n * pow(stat.mu_0 - stat.mu, 2.0))                            \
    + pow(val - stat.mu, 2.0)) / stat.n)                                  \
    : (pow(val - stat.mu, 2.0) / (stat.n + 1)),                           \
  stat.sigma = sqrt(stat.var),                                            \
  stat.n++

#if 0
#define stat_show(stat, name, unit) \
    log_infof("    %s (%s):                               \r\n", name, unit); \
    log_infof("         N:                              %.0f\r\n", stat.n);   \
    log_infof("         μ:                              %f\r\n", stat.mu);    \
    log_infof("         σ:                              %f\r\n", stat.sigma)
#endif

#define stat_show(stat, name, unit) \
  log_infof("%s (%s): %.0f, %f, %f\r\n", name, unit, stat.n, stat.mu, stat.sigma)

#endif
