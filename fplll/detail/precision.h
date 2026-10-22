#ifndef FPLLL_DETAIL_PRECISION_H
#define FPLLL_DETAIL_PRECISION_H

#include "../nr/nr.h"

FPLLL_BEGIN_NAMESPACE

/** Restores the previous MPFR default precision when leaving a scope. */
class ScopedPrecision
{
public:
  explicit ScopedPrecision(unsigned int precision)
      : old_precision_(FP_NR<mpfr_t>::set_prec(precision))
  {
  }

  ~ScopedPrecision() noexcept { FP_NR<mpfr_t>::set_prec(old_precision_); }

  ScopedPrecision(const ScopedPrecision &)            = delete;
  ScopedPrecision &operator=(const ScopedPrecision &) = delete;

private:
  unsigned int old_precision_;
};

FPLLL_END_NAMESPACE

#endif
