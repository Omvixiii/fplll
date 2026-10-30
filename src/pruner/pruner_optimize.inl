// SPDX-FileCopyrightText: 2016-2017 Leo Ducas
// SPDX-FileCopyrightText: 2018 Shi Bai
//
// SPDX-License-Identifier: LGPL-2.1-or-later
/** \file pruner_optimize.inl
    Top-level optimization drivers of the pruner. */

namespace fplll
{

namespace detail
{
inline constexpr int num_optimization_tours = 3;
}  // namespace detail

template <class FT> void Pruner<FT>::optimize_coefficients_cost_vary_prob(std::vector<double> &pr)
{
  FT old_c0, new_c, min_c;
  vec b(n), best_b(n);

  // step 1: preparation
  optimize_coefficients_preparation(pr);

  // step 2: optimization using half the coefficients only
  optimize_coefficients_evec_core(pr);

  load_coefficients(b, pr);
  best_b = b;
  old_c0 = target_function(b);
  min_c  = old_c0;

  // step 3: use full coefficients if enabled
  if (!options.half)
  {
    int tours = 0;
    while (true)
    {
      tours++;

      // initial cost
      load_coefficients(b, pr);
      old_c0 = target_function(b);

      // step 3.1: full tuning
      optimize_coefficients_local_adjust_decr_single(pr);
      optimize_coefficients_local_adjust_incr_prob(pr);
      optimize_coefficients_local_adjust_smooth(pr);

      // update best after tuning
      load_coefficients(b, pr);
      new_c = target_function(b);
      if (new_c < min_c)
      {
        min_c  = new_c;
        best_b = b;
      }

      // step 3.2: full optimization
      optimize_coefficients_full_core(pr);

      load_coefficients(b, pr);
      new_c = target_function(b);
      if (new_c < min_c)
      {
        min_c  = new_c;
        best_b = b;
      }

      // break if not improving
      if (new_c / old_c0 > 0.995 && tours > detail::num_optimization_tours)
      {
        break;
      }
    }
    save_coefficients(pr, best_b);
  }
  else
  {
    save_coefficients(pr, b);
  }
}

template <class FT> void Pruner<FT>::optimize_coefficients_cost_fixed_prob(std::vector<double> &pr)
{
  vec b(n);
  FT prob;

  // step 1: global optimization (without fixing the success probability)
  optimize_coefficients_preparation(pr);
  optimize_coefficients_evec_core(pr);
  optimize_coefficients_local_adjust_smooth(pr);
  optimize_coefficients_full_core(pr);

  // step 2: achieve the target success probability
  optimize_coefficients_local_adjust_smooth(pr);
  load_coefficients(b, pr);
  prob = measure_metric(b);
  if (prob <= target)
  {
    optimize_coefficients_incr_prob(pr);
  }
  else
  {
    optimize_coefficients_decr_prob(pr);
  }

  // step 3: some local tweaking
  optimize_coefficients_local_adjust_smooth(pr);
  optimize_coefficients_local_adjust_prob(pr);
}

/** Optimizes either the overall cost or the cost under a fixed probability. */
template <class FT> void Pruner<FT>::optimize_coefficients(std::vector<double> &pr)
{
  if (opt_single)
  {
    optimize_coefficients_cost_fixed_prob(pr);
  }
  else
  {
    optimize_coefficients_cost_vary_prob(pr);
  }
}

}  // namespace fplll
