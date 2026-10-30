// SPDX-FileCopyrightText: 2014 Damien Stehle
// SPDX-FileCopyrightText: 2016 Shi Bai
// SPDX-FileCopyrightText: 2016-2019 Martin R. Albrecht
// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef FPLLL_NR_H
#define FPLLL_NR_H

/**
 * Loading definitions.
 *
 * The numeric layer is the only part of the library that depends on GMP and
 * MPFR, so their headers are included here and nowhere else in the public
 * headers.  The class templates live in the implementation includes below;
 * the IWYU export pragmas mark them as part of this public header so that
 * users of FP_NR, Z_NR, RandGen and the GMP/MPFR types only need to include
 * this header directly (include-cleaner would otherwise point at the
 * implementation includes, which are not meant to be included by hand).
 */
#include <gmp.h>   // IWYU pragma: export
#include <mpfr.h>  // IWYU pragma: export

#include <iostream>  // IWYU pragma: keep

#include "../defs.h"  // IWYU pragma: keep

#include "nr_rand.inl"  // IWYU pragma: export

#include "nr_FP.inl"     // IWYU pragma: export
#include "nr_FP_d.inl"   // IWYU pragma: export
#include "nr_Z.inl"      // IWYU pragma: export
#include "nr_Z_l.inl"    // IWYU pragma: export
#include "nr_Z_mpz.inl"  // IWYU pragma: export

#ifdef FPLLL_WITH_QD
#include "nr_FP_dd.inl"  // IWYU pragma: export
#include "nr_FP_qd.inl"  // IWYU pragma: export
#endif

#include "nr_FP_mpfr.inl"  // IWYU pragma: export

#include "nr_FP_misc.inl"  // IWYU pragma: export
#include "nr_Z_misc.inl"   // IWYU pragma: export

namespace fplll
{

/**
 * Returns a printable name for the numeric type T. Used for diagnostics
 * only; unknown types have an empty name.
 */
template <class T> inline const char *num_type_str() { return ""; }
template <> inline const char *num_type_str<long>() { return "long"; }
template <> inline const char *num_type_str<double>() { return "double"; }
template <> inline const char *num_type_str<mpz_t>() { return "mpz_t"; }
#ifdef FPLLL_WITH_QD
template <> inline const char *num_type_str<dd_real>() { return "dd_real"; }
template <> inline const char *num_type_str<qd_real>() { return "qd_real"; }
#endif
template <> inline const char *num_type_str<mpfr_t>() { return "mpfr_t"; }

}  // namespace fplll

#endif
