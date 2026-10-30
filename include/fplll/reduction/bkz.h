// SPDX-FileCopyrightText: 2011 Xavier Pujol
// SPDX-FileCopyrightText: 2014-2016 Martin R. Albrecht
// SPDX-FileCopyrightText: 2016 Michael Walter
// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef FPLLL_BKZ_H
#define FPLLL_BKZ_H

#include <gmp.h>

#include "../defs.h"
#include "../nr/matrix.h"
#include "bkz_param.h"

namespace fplll
{

/**
   @brief Performs block reduction on the basis `b`.

   The parameters and the selected floating-point backend are validated before
   `b` is modified, so an invalid `BKZParam` throws `fplll::Error` and leaves
   the basis untouched.  `BKZVariant::sd_bkz` additionally requires
   `block_size` not to exceed the number of non-zero rows of `b` when the
   basis has at least two non-zero rows (smaller bases are reduced trivially
   without touching the tour bounds).

   @param b      basis of the lattice to be reduced
   @param param  block-reduction parameters
   @return       the report: status, backend/precision actually used and the
                 number of completed tours/nodes; `guaranteed` is always false
                 for BKZ
*/
[[nodiscard]] ReductionReport bkz_reduction(ZZ_mat<mpz_t> &b, const BKZParam &param);

/**
   @brief Performs block reduction and stores the transformation matrix in `u`.

   `u` is overwritten with the unimodular transformation that maps the input
   basis to the reduced one.  As above, the parameters are validated before
   `b` or `u` is modified; an invalid `BKZParam` throws `fplll::Error` and
   leaves both untouched.

   @param b      basis of the lattice to be reduced
   @param u      transformation matrix, overwritten
   @param param  block-reduction parameters
   @return       the report: status, backend/precision actually used and the
                 number of completed tours/nodes; `guaranteed` is always false
                 for BKZ
*/
[[nodiscard]] ReductionReport bkz_reduction(ZZ_mat<mpz_t> &b, ZZ_mat<mpz_t> &u,
                                            const BKZParam &param);

/** Options of the HKZ entry point. */
struct HKZOptions
{
  /** Floating-point backend; `automatic` selects one from the precision. */
  FloatType float_type = FloatType::automatic;
  /** MPFR precision.  A non-zero value selects the MPFR backend at that
      precision (also with `automatic`).  0 means "not specified": with
      `automatic` the run stays in double precision, and with an explicit
      `FloatType::mpfr` the current process default MPFR precision is used. */
  int precision = 0;
  /** Print progress information to log_stream(). */
  bool verbose = false;
};

/**
   @brief Performs HKZ reduction on the basis `b`.

   @param b        basis of the lattice to be reduced
   @param options  reduction options
   @return         the report: status, backend/precision actually used and the
                   number of completed tours/nodes; `guaranteed` is always
                   false for HKZ
*/
[[nodiscard]] ReductionReport hkz_reduction(ZZ_mat<mpz_t> &b, const HKZOptions &options = {});

}  // namespace fplll

#endif
