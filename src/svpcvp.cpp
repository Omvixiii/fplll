// SPDX-FileCopyrightText: 2008-2011 Xavier Pujol
// SPDX-FileCopyrightText: 2015 Michael Walter
// SPDX-FileCopyrightText: 2019 Koen de Boer & Wessel van Woerden
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "detail/float_type.h"
#include <fplll/defs.h>
#include <fplll/enum/enumerate.h>
#include <fplll/enum/evaluator.h>
#include <fplll/gso/gso.h>
#include <fplll/gso/gso_interface.h>
#include <fplll/nr/matrix.h>
#include <fplll/nr/nr.h>
#include <fplll/nr/numvect.h>
#include <fplll/svpcvp.h>
#include <fplll/util.h>

#include <algorithm>
#include <cstddef>
#include <gmp.h>
#include <memory>
#include <utility>
#include <vector>

namespace fplll
{

namespace
{

/* Returns i such that the shortest vector of L(b) belongs to
   L(b_0,...,b_(i-1)), assuming that the error on rdiag's is less than 100%.
   If b is LLL-reduced, then for any reasonable dimension,
   max(rdiag[0],...,rdiag[i-1]) / min(rdiag[0],...,rdiag[i-1])
   is much smaller than numeric_limits<double>::max */
int last_useful_index(const Matrix<FP_NR<mpfr_t>> &r)
{
  int i;
  FP_NR<mpfr_t> rdiag_min_value;
  rdiag_min_value.mul_2si(r(0, 0), 1);
  for (i = r.get_rows() - 1; i > 0; i--)
  {
    if (r(i, i) <= rdiag_min_value)
    {
      break;
    }
  }
  return i + 1;
}

/* Returns the squared norm of the shortest basis vector among
   b[first], ..., b[last-1], using the exact integer Gram matrix of the GSO. */
void get_basis_min(Z_NR<mpz_t> &basis_min, MatGSOInterface<Z_NR<mpz_t>, FP_NR<mpfr_t>> &gso,
                   int first, int last)
{
  Z_NR<mpz_t> sq_norm;
  gso.get_int_gram(basis_min, first, first);
  for (int i = first + 1; i < last; i++)
  {
    gso.get_int_gram(sq_norm, i, i);
    if (sq_norm < basis_min)
    {
      basis_min = sq_norm;
    }
  }
}

/* Enumerates the shortest vector(s) of the lattice described by `gso`. */
void enumerate_svp(int d, MatGSOInterface<Z_NR<mpz_t>, FP_NR<mpfr_t>> &gso, FP_NR<mpfr_t> &max_dist,
                   ErrorBoundedEvaluator &evaluator, const SVPOptions &options)
{
  Enumeration<Z_NR<mpz_t>, FP_NR<mpfr_t>> enumobj(gso, evaluator);
  enumobj.enumerate(0, d, max_dist, 0, std::vector<FP_NR<mpfr_t>>(), std::vector<enumf>(),
                    options.dual);
}

/* Common implementation of the SVP solvers working on a GSO object.  The
   optional `auxsol_*` arguments collect the non-shortest solutions found (used
   by the list-SVP interface). */
RedStatus shortest_vector_impl(MatGSOInterface<Z_NR<mpz_t>, FP_NR<mpfr_t>> &gso,
                               std::vector<Z_NR<mpz_t>> &sol_coord, const SVPOptions &options,
                               std::vector<std::vector<Z_NR<mpz_t>>> *auxsol_coord = nullptr,
                               std::vector<enumf> *auxsol_dist = nullptr, int max_aux_sols = 0,
                               bool merge_shortest = false)
{
  // d = lattice dimension (note that it might decrease during preprocessing)
  int d = gso.get_d();
  // n = dimension of the space
  int n = gso.get_cols_of_b();

  FPLLL_CHECK(d > 0 && n > 0, "shortest_vector: empty matrix");
  FPLLL_CHECK(d <= n, "shortest_vector: number of vectors > size of the vectors");

  // Sets the floating-point precision
  // Error bounds on GSO are valid if prec >= minprec
  double rho;
  int min_prec = gso_min_prec(rho, d, LLL_DEF_DELTA, LLL_DEF_ETA);
  int prec     = std::max(53, min_prec + 10);
  ScopedPrecision precision_guard(static_cast<unsigned int>(prec));

  FP_NR<mpfr_t> max_dist;
  Z_NR<mpz_t> int_max_dist;
  Z_NR<mpz_t> itmp1;

  // Computes the Gram-Schmidt orthogonalization in floating-point
  gso.update_gso();
  gen_zero_vect(sol_coord, d);

  // If the last b_i* are too large, removes them to avoid an underflow
  int new_d = last_useful_index(gso.get_r_matrix());
  if (new_d < d)
  {
    d = new_d;
  }

  if (options.dual)
  {
    max_dist = 1.0 / gso.get_r_exp(d - 1, d - 1);
    if (options.verbose)
    {
      log_stream() << "max_dist = " << max_dist << '\n';
    }
  }
  else
  {
    /* Computes a bound for the enumeration. This bound would work for an
       exact algorithm, but we will increase it later to ensure that the fp
       algorithm finds a solution */
    get_basis_min(int_max_dist, gso, 0, d);
    max_dist.set_z(int_max_dist, GMP_RNDU);
  }

  // Initializes the evaluator of solutions
  const auto nr_solutions = static_cast<std::size_t>(max_aux_sols) + 1;
  std::unique_ptr<ErrorBoundedEvaluator> evaluator;
  if (options.method == SVPMethod::fast)
  {
    evaluator = std::make_unique<FastErrorBoundedEvaluator>(
        d, gso.get_mu_matrix(), gso.get_r_matrix(), EvaluatorMode::sv, nr_solutions,
        EvaluatorStrategy::best_n_solutions, false);
  }
  else
  {
    auto exact = std::make_unique<ExactErrorBoundedEvaluator>(
        d, gso, EvaluatorMode::sv, nr_solutions, EvaluatorStrategy::best_n_solutions, false);
    exact->int_max_dist = int_max_dist;
    evaluator           = std::move(exact);
  }
  evaluator->init_delta_def(prec, rho, true);

  if (!options.override_bound)
  {
    FP_NR<mpfr_t> ftmp1;
    bool result = evaluator->get_max_error_aux(max_dist, true, ftmp1);
    FPLLL_CHECK(result, "shortest_vector: cannot compute an initial bound");
    max_dist.add(max_dist, ftmp1, GMP_RNDU);
  }

  // Main loop of the enumeration
  enumerate_svp(d, gso, max_dist, *evaluator, options);

  RedStatus result = RedStatus::enum_failure;
  if (!evaluator->empty())
  {
    for (int i = 0; i < d; i++)
    {
      itmp1.set_f(evaluator->begin()->second[i]);
      sol_coord[i].add(sol_coord[i], itmp1);
    }
    result = RedStatus::success;
  }
  else
  {
    // A failed search must not leave a zero vector that looks like a result.
    sol_coord.clear();
  }

  if (auxsol_coord != nullptr && auxsol_dist != nullptr)
  {
    auxsol_coord->clear();
    auxsol_dist->clear();
    if (result == RedStatus::success)
    {
      // Iterators over all solutions; optionally skip the shortest one.  With
      // max_aux_sols == 0 the shortest solution is still returned (the caller
      // asked for max_sols == 1), so the output arrays are always filled.
      auto it = evaluator->begin(), itend = evaluator->end();
      if (!merge_shortest)
      {
        ++it;
      }
      for (; it != itend; ++it)
      {
        auxsol_dist->push_back(it->first.get_d());

        std::vector<Z_NR<mpz_t>> as_c;
        for (const auto &j : it->second)
        {
          itmp1.set_f(j);
          as_c.emplace_back(itmp1);
        }
        auxsol_coord->emplace_back(std::move(as_c));
      }
    }
  }

  return result;
}

}  // namespace

RedStatus shortest_vector(ZZ_mat<mpz_t> &b, std::vector<Z_NR<mpz_t>> &sol_coord,
                          const SVPOptions &options)
{
  FPLLL_CHECK(b.get_rows() > 0 && b.get_cols() > 0, "shortest_vector: empty matrix");
  FPLLL_CHECK(b.get_rows() <= b.get_cols(),
              "shortest_vector: number of vectors > size of the vectors");

  // Allocates space for vectors and matrices in constructors
  ZZ_mat<mpz_t> empty_mat;
  MatGSO<Z_NR<mpz_t>, FP_NR<mpfr_t>> gso(b, empty_mat, empty_mat, GSOOptions{.int_gram = true});
  return shortest_vector_impl(gso, sol_coord, options);
}

RedStatus shortest_vector(MatGSOInterface<Z_NR<mpz_t>, FP_NR<mpfr_t>> &gso,
                          std::vector<Z_NR<mpz_t>> &sol_coord, const SVPOptions &options)
{
  return shortest_vector_impl(gso, sol_coord, options);
}

RedStatus shortest_vectors(MatGSOInterface<Z_NR<mpz_t>, FP_NR<mpfr_t>> &gso,
                           std::vector<std::vector<Z_NR<mpz_t>>> &sol_coord,
                           std::vector<enumf> &sol_dist, const int max_sols,
                           const SVPOptions &options)
{
  FPLLL_CHECK(max_sols > 0, "shortest_vectors: max_sols must be strictly positive");
  std::vector<Z_NR<mpz_t>> sol_coord_tmp;
  return shortest_vector_impl(gso, sol_coord_tmp, options, &sol_coord, &sol_dist, max_sols - 1,
                              true);
}

/* Closest vector problem
   ====================== */

namespace
{

void get_gscoords(const Matrix<FP_NR<mpfr_t>> &matrix, const Matrix<FP_NR<mpfr_t>> &mu,
                  const Matrix<FP_NR<mpfr_t>> &r, const std::vector<FP_NR<mpfr_t>> &v,
                  std::vector<FP_NR<mpfr_t>> &vcoord)
{
  int n = matrix.get_rows(), m = matrix.get_cols();

  if (std::cmp_not_equal(vcoord.size(), n))
  {
    vcoord.resize(n);
  }
  FPLLL_DEBUG_CHECK(mu.get_rows() == n && mu.get_cols() == n && r.get_rows() == n &&
                    r.get_cols() == n && std::cmp_equal(v.size(), m));

  for (int i = 0; i < n; i++)
  {
    vcoord[i] = 0.0;
    for (int j = 0; j < m; j++)
    {
      vcoord[i].addmul(v[j], matrix(i, j));
    }
    for (int j = 0; j < i; j++)
    {
      vcoord[i].submul(mu(i, j), vcoord[j]);
    }
  }
  for (int i = 0; i < n; i++)
  {
    vcoord[i].div(vcoord[i], r(i, i));
  }
}

void babai(const FP_mat<mpfr_t> &matrix, const Matrix<FP_NR<mpfr_t>> &mu,
           const Matrix<FP_NR<mpfr_t>> &r, const std::vector<FP_NR<mpfr_t>> &target,
           std::vector<FP_NR<mpfr_t>> &target_coord)
{
  int d = matrix.get_rows();
  get_gscoords(matrix, mu, r, target, target_coord);
  for (int i = d - 1; i >= 0; i--)
  {
    target_coord[i].rnd(target_coord[i]);
    for (int j = 0; j < i; j++)
    {
      target_coord[j].submul(mu(i, j), target_coord[i]);
    }
  }
}

}  // namespace

RedStatus closest_vector(ZZ_mat<mpz_t> &b, const std::vector<Z_NR<mpz_t>> &int_target,
                         std::vector<Z_NR<mpz_t>> &sol_coord, const CVPOptions &options)
{
  // d = lattice dimension (note that it might decrease during preprocessing)
  int d = b.get_rows();
  // n = dimension of the space
  int n = b.get_cols();

  // All precondition checks happen before sol_coord is written: a violating
  // call must leave the caller's output vector untouched (svpcvp.h contract).
  FPLLL_CHECK(d > 0 && n > 0, "closest_vector: empty matrix");
  FPLLL_CHECK(d <= n, "closest_vector: number of vectors > size of the vectors");
  FPLLL_CHECK(std::cmp_equal(int_target.size(), static_cast<std::size_t>(n)),
              "closest_vector: target dimension mismatch (expected {}, got {})", n,
              int_target.size());

  // Sets the floating-point precision
  // Error bounds on GSO are valid if prec >= minprec
  double rho;
  int min_prec = gso_min_prec(rho, d, LLL_DEF_DELTA, LLL_DEF_ETA);
  int prec     = std::max(53, min_prec + 10);
  ScopedPrecision precision_guard(static_cast<unsigned int>(prec));

  // Allocates space for vectors and matrices in constructors
  ZZ_mat<mpz_t> empty_mat;
  MatGSO<Z_NR<mpz_t>, FP_NR<mpfr_t>> gso(b, empty_mat, empty_mat, GSOOptions{.int_gram = true});
  std::vector<FP_NR<mpfr_t>> target_coord;
  FP_NR<mpfr_t> max_dist;
  Z_NR<mpz_t> itmp1;

  // Computes the Gram-Schmidt orthogonalization in floating-point
  gso.update_gso();
  gen_zero_vect(sol_coord, d);

  /* Applies Babai's algorithm. Because we use fp, it might be necessary to
     do it several times (if ||target|| >> ||b_i||) */
  FP_mat<mpfr_t> float_matrix(d, n);
  std::vector<FP_NR<mpfr_t>> target(n), babai_sol;
  std::vector<Z_NR<mpz_t>> int_new_target = int_target;

  for (int i = 0; i < d; i++)
  {
    for (int j = 0; j < n; j++)
    {
      float_matrix(i, j).set_z(b(i, j));
    }
  }

  for (int loop_idx = 0;; loop_idx++)
  {
    if (loop_idx >= 0x100 && ((loop_idx & (loop_idx - 1)) == 0))
    {
      log_stream() << "warning: possible infinite loop in Babai's algorithm" << '\n';
    }

    for (int i = 0; i < n; i++)
    {
      target[i].set_z(int_new_target[i]);
    }
    babai(float_matrix, gso.get_mu_matrix(), gso.get_r_matrix(), target, babai_sol);
    int idx;
    for (idx = 0; idx < d && babai_sol[idx] >= -1 && babai_sol[idx] <= 1; idx++)
    {
    }
    if (idx == d)
    {
      break;
    }

    for (int i = 0; i < d; i++)
    {
      itmp1.set_f(babai_sol[i]);
      sol_coord[i].add(sol_coord[i], itmp1);
      for (int j = 0; j < n; j++)
      {
        int_new_target[j].submul(itmp1, b(i, j));
      }
    }
  }
  get_gscoords(float_matrix, gso.get_mu_matrix(), gso.get_r_matrix(), target, target_coord);

  /* Computes a very large bound to make the algorithm work
     until the first solution is found.  The sum over the r(i, i) below is empty
     for d == 1, which used to leave max_dist at 0 and made every
     one-dimensional CVP fail.  For d == 1 Babai's residual GS coordinate is in
     [-1/2, 1/2], so its squared distance is at most r(0, 0) / 4 and r(0, 0)
     (the squared norm of the single basis vector) is a safe bound. */
  max_dist = 0.0;
  for (int i = 1; i < d; i++)
  {
    // get_r_exp(i, i) = r(i, i) because gso is initialized without the row-exponent option
    max_dist.add(max_dist, gso.get_r_exp(i, i));
  }
  if (d == 1)
  {
    max_dist = gso.get_r_exp(0, 0);
  }

  std::vector<int> max_indices;
  if (options.method == CVPMethod::proved)
  {
    // For exact CVP, we need to reset enum below depth with maximal r_i
    max_indices = std::vector<int>(d);
    int cur, max_index, previous_max_index;
    previous_max_index = max_index = d - 1;
    FP_NR<mpfr_t> max_val;

    while (max_index > 0)
    {
      max_val = gso.get_r_exp(max_index, max_index);
      for (cur = previous_max_index - 1; cur >= 0; --cur)
      {
        if (max_val <= gso.get_r_exp(cur, cur))
        {
          max_val   = gso.get_r_exp(cur, cur);
          max_index = cur;
        }
      }
      for (cur = max_index; cur < previous_max_index; ++cur)
      {
        max_indices[cur] = max_index;
      }
      max_indices[previous_max_index] = previous_max_index;
      previous_max_index              = max_index;
      --max_index;
    }
  }
  FastErrorBoundedEvaluator evaluator(d, gso.get_mu_matrix(), gso.get_r_matrix(),
                                      EvaluatorMode::cv);

  // Main loop of the enumeration
  Enumeration<Z_NR<mpz_t>, FP_NR<mpfr_t>> enumobj(gso, evaluator, max_indices);
  enumobj.enumerate(0, d, max_dist, 0, target_coord);

  RedStatus result = RedStatus::enum_failure;
  if (!evaluator.empty())
  {
    if (options.verbose)
    {
      log_stream() << "max_dist=" << max_dist << '\n';
    }
    for (int i = 0; i < d; i++)
    {
      itmp1.set_f(evaluator.begin()->second[i]);
      sol_coord[i].add(sol_coord[i], itmp1);
    }
    result = RedStatus::success;
  }

  if (result != RedStatus::success)
  {
    // Match the contract of shortest_vector() and the documented CVP contract:
    // a failed solve (currently enum_failure, the only failure status this
    // entry point can return) must not leave the partial Babai coordinates
    // behind as if they were a result.
    sol_coord.clear();
  }

  return result;
}

}  // namespace fplll
