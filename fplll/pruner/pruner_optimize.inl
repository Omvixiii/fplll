#include "../fplll.h"

FPLLL_BEGIN_NAMESPACE

#define NUM_OPTIMIZATION_TOURS 3

template <class FT> void Pruner<FT>::optimize_coefficients_cost_vary_prob(/*io*/ vector<double> &pr)
{

  FT old_c0, old_c1, new_c, min_c;
  vec b(n), best_b(n);

  // step 1 preparation
  optimize_coefficients_preparation(pr);

  // step 2 optimization use half coefficients only
  optimize_coefficients_evec_core(pr);

  load_coefficients(b, pr);
  best_b = b;
  old_c0 = target_function(b);
  min_c  = old_c0;

  // step 2 use full coefficients if enabled
  if (!(flags & PRUNER_HALF))
  {
    int tours = 0;
    while (1)
    {
      tours++;

      // initial cost
      load_coefficients(b, pr);
      old_c0 = target_function(b);

      // step 2.1 full tuning
      optimize_coefficients_local_adjust_decr_single(pr);
      optimize_coefficients_local_adjust_incr_prob(pr);
      optimize_coefficients_local_adjust_smooth(pr);

      // update best after tuning
      load_coefficients(b, pr);
      old_c1 = target_function(b);
      if (old_c1 < min_c)
      {
        min_c  = old_c1;
        best_b = b;
      }

      // step 2.2 full optimization
      optimize_coefficients_full_core(pr);

      load_coefficients(b, pr);
      new_c = target_function(b);
      if (new_c < min_c)
      {
        min_c  = new_c;
        best_b = b;
      }

      // break if not improving
      if (new_c / old_c0 > 0.995 and tours > NUM_OPTIMIZATION_TOURS)
        break;
    }
    save_coefficients(pr, best_b);
  }
  else
  {
    save_coefficients(pr, b);
  }
}

template <class FT>
void Pruner<FT>::optimize_coefficients_cost_fixed_prob(/*io*/ vector<double> &pr)
{
  vec b(n);
  FT prob;

  // step 1 call global optimization (without fixing succ. prob)
  optimize_coefficients_preparation(pr);
  optimize_coefficients_evec_core(pr);
  optimize_coefficients_local_adjust_smooth(pr);
  optimize_coefficients_full_core(pr);

  // step 2 achieve target succ. prob
  optimize_coefficients_local_adjust_smooth(pr);
  load_coefficients(b, pr);
  prob = measure_metric(b);
  if (prob <= target)
    optimize_coefficients_incr_prob(pr);
  else
  {
    optimize_coefficients_decr_prob(pr);
  }

  // step 3: some local tweaking
  optimize_coefficients_local_adjust_smooth(pr);
  optimize_coefficients_local_adjust_prob(pr);
}

/**
 *  optimize either overall cost or cost by fixing probability
 */
template <class FT> void Pruner<FT>::optimize_coefficients(/*io*/ vector<double> &pr)
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

FPLLL_END_NAMESPACE
