// SPDX-FileCopyrightText: 2008-2011 Xavier Pujol
// SPDX-FileCopyrightText: 2019 Koen de Boer & Wessel van Woerden
//
// SPDX-License-Identifier: LGPL-2.1-or-later
/** \file svpcvp.h
    Shortest vector problem. */

#ifndef FPLLL_SVPCVP_H
#define FPLLL_SVPCVP_H

#include "defs.h"
#include "gso/gso_interface.h"
#include "nr/matrix.h"
#include "nr/nr.h"

#include <gmp.h>
#include <vector>

namespace fplll
{

/** Options of the SVP solvers. */
struct SVPOptions
{
  /** Solver variant. */
  SVPMethod method = SVPMethod::proved;
  /** Enumerate the dual lattice. */
  bool dual = false;
  /** Trust the caller's enumeration bound instead of enlarging it for the
      floating-point error. */
  bool override_bound = false;
  /** Print solver diagnostics to `log_stream()`. */
  bool verbose = false;
};

/** Options of the CVP solvers. */
struct CVPOptions
{
  /** Solver variant. */
  CVPMethod method = CVPMethod::proved;
  /** Print solver diagnostics to `log_stream()`. */
  bool verbose = false;
};

/**
 * Computes a shortest vector of a lattice.
 * The vectors must be linearly independent and the basis must be LLL-reduced
 * with delta = LLL_DEF_DELTA and eta = LLL_DEF_ETA.
 * The result is guaranteed if `options.method == SVPMethod::proved`.
 *
 * On success `sol_coord` holds the coordinates of one shortest vector; on
 * failure it is emptied so a stale zero vector cannot be mistaken for a
 * result.
 *
 * @param b input lattice
 * @param sol_coord coordinates of the shortest vector in the input basis
 * @param options solver options
 * @return the status of the solver
 */
[[nodiscard]] RedStatus shortest_vector(ZZ_mat<mpz_t> &b, std::vector<Z_NR<mpz_t>> &sol_coord,
                                        const SVPOptions &options = {});

/**
 * Computes a shortest vector of a lattice described by a GSO object.
 * The basis of the GSO object must be LLL-reduced.
 *
 * @param gso Gram-Schmidt orthogonalization of the input lattice
 * @param sol_coord coordinates of the shortest vector in the input basis
 * @param options solver options
 * @return the status of the solver
 */
[[nodiscard]] RedStatus shortest_vector(MatGSOInterface<Z_NR<mpz_t>, FP_NR<mpfr_t>> &gso,
                                        std::vector<Z_NR<mpz_t>> &sol_coord,
                                        const SVPOptions &options = {});

/**
 * Computes the `max_sols` shortest vectors of a lattice described by a GSO
 * object.
 *
 * On success `sol_coord` and `sol_dist` contain the same number of entries:
 * the vectors found, at most `max_sols` and at least one (also for
 * `max_sols == 1`).  They are emptied on failure.
 *
 * @param gso Gram-Schmidt orthogonalization of the input lattice
 * @param sol_coord coordinates of the shortest vectors in the input basis
 * @param sol_dist squared lengths of the returned vectors
 * @param max_sols maximal number of vectors to return
 * @param options solver options
 * @return the status of the solver
 */
[[nodiscard]] RedStatus shortest_vectors(MatGSOInterface<Z_NR<mpz_t>, FP_NR<mpfr_t>> &gso,
                                         std::vector<std::vector<Z_NR<mpz_t>>> &sol_coord,
                                         std::vector<enumf> &sol_dist, int max_sols,
                                         const SVPOptions &options = {});

/**
 * Computes a closest vector of a lattice to a target.
 * The vectors must be linearly independent and the basis must be LLL-reduced
 * with delta = LLL_DEF_DELTA and eta = LLL_DEF_ETA.
 * The result is guaranteed if `options.method == CVPMethod::proved`.
 *
 * The preconditions are validated before `sol_coord` is touched: `b` must be
 * non-empty with at least as many columns as rows, and `int_target` must have
 * exactly `b.get_cols()` coordinates.  A violation throws `fplll::Error` and
 * leaves `sol_coord` exactly as the caller passed it, so a stale result is
 * never exposed; in particular a target of the wrong dimension is rejected
 * instead of being read out of bounds or silently truncated.
 *
 * Solver failures are reported through the return status, not by throwing: on
 * success `sol_coord` holds the coordinates of one closest vector in the input
 * basis; on a failure status it is emptied, so a stale or partial coordinate
 * vector cannot be mistaken for a result.
 *
 * @param b input lattice
 * @param int_target target vector, with `b.get_cols()` coordinates
 * @param sol_coord coordinates of the closest vector in the input basis
 * @param options solver options
 * @return the status of the solver
 */
[[nodiscard]] RedStatus closest_vector(ZZ_mat<mpz_t> &b, const std::vector<Z_NR<mpz_t>> &int_target,
                                       std::vector<Z_NR<mpz_t>> &sol_coord,
                                       const CVPOptions &options = {});

}  // namespace fplll

#endif
