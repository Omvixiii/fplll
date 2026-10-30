// SPDX-FileCopyrightText: 2016-2017 Leo Ducas
// SPDX-FileCopyrightText: 2018 Shi Bai
//
// SPDX-License-Identifier: LGPL-2.1-or-later
/** \file pruner_optimize_tc.inl
    Optimization of the enumeration cost. */

namespace fplll
{

namespace detail
{
inline constexpr int nd_alpha         = 1;     ///< Nelder-Mead reflection coefficient
inline constexpr int nd_gamma         = 2;     ///< Nelder-Mead expansion coefficient
inline constexpr double nd_rho        = 0.5;   ///< Nelder-Mead contraction coefficient
inline constexpr double nd_sigma      = 0.5;   ///< Nelder-Mead shrink coefficient
inline constexpr double nd_init_width = 0.01;  ///< Initial simplex width
}  // namespace detail

/** Preparation work to obtain raw pruning coefficients. */
template <class FT> void Pruner<FT>::optimize_coefficients_preparation(std::vector<double> &pr)
{
  evec b(d);

  // load coefficients
  if (options.start_from_input)
  {
    load_coefficients(b, pr);
  }
  else
  {
    // greedy method
    greedy(b);
  }

  // greedy method for the minimum pruning coefficients
  {
    preproc_cost *= .1;
    greedy(min_pruning_coefficients);

    // The aim is to get a lower bound for the pruning parameter; note this
    // lower bound will be used in enforce().  In the case of a fixed input
    // probability, check whether this min_pruning_coefficients is small
    // enough. Otherwise, reduce it further. This is important since otherwise
    // one may never achieve the target probability.
    if (!opt_single)
    {
      std::vector<double> pr_min(n);
      save_coefficients(pr_min, min_pruning_coefficients);
      if (measure_metric(min_pruning_coefficients) > target)
      {
        std::fill(min_pruning_coefficients.begin(), min_pruning_coefficients.end(), 0.);
        optimize_coefficients_decr_prob(pr_min);
      }
      load_coefficients(min_pruning_coefficients, pr_min);
    }
    preproc_cost *= 10;
  }
  save_coefficients(pr, b);
}

/** Optimizes with the constraint that b_i = b_{i+1} for even i. */
template <class FT> void Pruner<FT>::optimize_coefficients_evec_core(std::vector<double> &pr)
{
  evec b(d);
  load_coefficients(b, pr);

  if (options.strategy == PrunerStrategy::gradient)
  {
    if (options.verbose)
    {
      log_stream() << "\nGradient descent start (dim=" << n << ")" << '\n';
    }
    gradient_descent(b);
  }

  if (options.strategy == PrunerStrategy::nelder_mead)
  {
    if (options.verbose)
    {
      log_stream() << "\nNelder-Mead start (dim=" << n << ")" << '\n';
    }
    while (nelder_mead_step(b))
    {
    }
  }
  save_coefficients(pr, b);
}

/** Optimizes without the constraint b_i = b_{i+1} for even i. */
template <class FT> void Pruner<FT>::optimize_coefficients_full_core(std::vector<double> &pr)
{
  vec b(n);

  // always load coefficients since this is used after optimize_coefficients_evec_core
  load_coefficients(b, pr);

  if (options.strategy == PrunerStrategy::gradient)
  {
    if (options.verbose)
    {
      log_stream() << "\nGradient descent start (dim=" << n << ")" << '\n';
    }
    gradient_descent(b);
  }

  if (options.strategy == PrunerStrategy::nelder_mead)
  {
    if (options.verbose)
    {
      log_stream() << "\nNelder-Mead start (dim=" << n << ")" << '\n';
    }
    while (nelder_mead_step(b))
    {
    }
  }

  save_coefficients(pr, b);
}

/** Tweaks the pruning coefficients in a neighborhood: tries to reduce the
    single-enumeration cost (hopefully reducing the overall running time). */
template <class FT>
void Pruner<FT>::optimize_coefficients_local_adjust_decr_single(std::vector<double> &pr)
{
  int maxi, lasti, consecutive_fails;
  double current_max = 0.0;
  FT old_cf, old_cfs, new_cf, old_b;
  std::vector<double> detailed_cost(n);
  std::vector<double> slices(n, 10.0);  // (b[i+1] - b[i])/slice will be used as step
  std::vector<int> thresholds(n, 3);
  vec b(n);

  load_coefficients(b, pr);

  lasti             = -1;  // last failed index; make sure we do not retry it immediately
  consecutive_fails = 0;   // number of consecutive failures; break when it reaches 10

  const double improved_ratio = 0.995;  // if reduced by 0.995, descend

  while (true)
  {
    // old cost
    old_cf = target_function(b);

    // find bottleneck index
    old_cfs = single_enum_cost(b, &detailed_cost);

    // heuristic
    if (old_cfs < sqrt(old_cf) / 10.0)
    {
      break;
    }

    current_max = 0.0;
    maxi        = 0;
    for (int i = 0; i < n; i++)
    {
      if ((i != (n - lasti - 1)) && (thresholds[n - i - 1] > 0) && (detailed_cost[i] > current_max))
      {
        current_max = detailed_cost[i];
        maxi        = i;
      }
    }

    // b[ind] is the one to be reduced
    int ind = n - maxi - 1;
    old_b   = b[ind];
    if (ind == 0)
    {
      break;
    }
    b[ind] = b[ind] - (b[ind] - b[ind - 1]) / slices[ind];

    // new cost
    new_cf = target_function(b);

    // if not improved -- recover
    if (new_cf >= (old_cf * improved_ratio))
    {
      b[ind] = old_b;
      lasti  = ind;
      thresholds[lasti]--;
      consecutive_fails++;
    }
    else
    {
      if (slices[ind] < 1024)
      {
        slices[ind] = slices[ind] * 1.05;
      }
      consecutive_fails = 0;
    }

    // quit after 10 consecutive failures
    if (consecutive_fails > 10)
    {
      break;
    }
  }

  save_coefficients(pr, b);
}

/** Tunes the pruning coefficients: tries to increase the success probability by
    increasing those coefficients b[i], inversely weighted by their level cost. */
template <class FT>
void Pruner<FT>::optimize_coefficients_local_adjust_incr_prob(std::vector<double> &pr)
{
  int trials, tours, maxi, ind;
  FT old_cf, old_cf0, old_cfs, new_cf, old_b;
  double current_max;
  std::vector<double> detailed_cost(n);
  std::vector<double> slices(n, 10.0);  // (b[i+1] - b[i])/slice will be used as step
  vec b(n);
  load_coefficients(b, pr);

  // initial cost
  old_cf0 = target_function(b);

  tours = 0;
  while (true)
  {
    tours++;

    // old cost
    old_cf = target_function(b);
    // find bottleneck index
    old_cfs     = single_enum_cost(b, &detailed_cost);
    current_max = 0.0;
    maxi        = 0;
    for (int i = 0; i < n; i++)
    {
      if (detailed_cost[i] > current_max)
      {
        current_max = detailed_cost[i];
        maxi        = i;
      }
    }
    ind = n - maxi - 1;
    if (ind <= 1)
    {
      break;
    }

    if (old_cfs > sqrt(old_cf) / 10.0)
    {
      break;
    }

    for (int i = ind; i >= 1; --i)
    {
      if (b[i] <= b[i - 1])
      {
        continue;
      }

      trials = 0;

      // fixed i-1, trying to increase b[i-1]
      while (true)
      {
        // old cost
        old_cf = target_function(b);

        // try increase
        old_b    = b[i - 1];
        b[i - 1] = b[i - 1] + (b[i] - b[i - 1]) / slices[i - 1];

        // new cost
        new_cf = target_function(b);

        // if not improved -- recover
        if (new_cf >= (old_cf * 1.2))
        {
          b[i - 1] = old_b;
          break;
        }
        if (slices[i - 1] < 1024)
        {
          slices[i - 1] = slices[i - 1] * 1.2;
        }
        trials++;
        if (trials >= 10)
        {
          break;
        }
      }
    }

    new_cf = target_function(b);
    if (new_cf > (old_cf0 * 1.1) || tours > 4)
    {
      break;
    }
  }

  save_coefficients(pr, b);
}

/** Tries to smooth the curve if there are obvious discontinuities between
    consecutive indices. */
template <class FT>
void Pruner<FT>::optimize_coefficients_local_adjust_smooth(std::vector<double> &pr)
{
  vec b(n);
  FT lr, rr;
  const FT th = 1.0 / n;
  load_coefficients(b, pr);

  for (int i = 1; i < n - 1; ++i)
  {
    lr = b[i] / b[i - 1];
    rr = b[i + 1] / b[i];

    if ((rr / lr > 1.25) || (rr / lr < 0.8))
    {
      b[i] = sqrt(b[i - 1] * b[i + 1]);
    }

    if ((b[i + 1] - b[i]) > th || (b[i] - b[i - 1]) > th)
    {
      b[i] = (b[i - 1] + b[i + 1]) / 2.0;
    }
  }

  save_coefficients(pr, b);
}

/** Optimizes b with the greedy method. */
template <class FT> void Pruner<FT>::greedy(evec &b)
{
  // Do not call enforce in this function, as min_pruning_coefficients may not
  // have been set: the min pruning bound is itself based on greedy.
  FPLLL_CHECK(shape_loaded, "Pruner: no basis shape was loaded");

  std::fill(min_pruning_coefficients.begin(), min_pruning_coefficients.end(), 0.);

  b.resize(d);
  std::fill(b.begin(), b.end(), 1.);
  FT nodes;

  for (int j = 1; j < 2 * d - 1; j += 2)
  {
    int i = j / 2;
    if (i > 1)
    {
      b[i] = b[i - 1] > .9 ? 1 : 1.1 * b[i - 1];
    }
    // Make the tree width a parabola, with maximum at n/2.  The integer
    // division is historical: promoting it to floating point would change the
    // pruning coefficients this heuristic produces, so keep the arithmetic.
    double goal_factor =
        1. / (3. * n) + 4 * j * (n - j) / (n * n * n);  // NOLINT(bugprone-integer-division)
    nodes = 1. + 1e10 * preproc_cost;

    while ((nodes > goal_factor * preproc_cost) && (b[i] > .001))
    {
      b[i] *= .98;
      for (int k = 0; k < i; ++k)
      {
        b[k] = b[k] < b[i] ? b[k] : b[i];  // enforce decreasing by hand
      }
      nodes = relative_volume((j + 1) / 2, b);
      nodes *= tabulated_ball_vol[j + 1];
      nodes *= pow_si(normalized_radius * sqrt(b[i]), j + 1);
      nodes *= ipv[j];
      nodes *= symmetry_factor;
    }
  }
}

/** Optimizes b with the gradient descent. */
template <class FT> int Pruner<FT>::gradient_descent(vec &b)
{
  FT old_epsilon  = epsilon;
  FT old_min_step = min_step;
  int trials      = 0;

  while (true)
  {
    int ret = gradient_descent_step(b);
    if (ret == 0)
    {
      break;
    }
    if (ret < 0)
    {
      epsilon  = epsilon * 0.9;
      min_step = min_step * 0.9;
      trials++;
      if (trials >= 5)
      {
        break;
      }
    }
    else
    {
      trials--;
      continue;
    }
  }

  epsilon  = old_epsilon;
  min_step = old_min_step;
  return 0;
}

/** One gradient descent step. */
template <class FT> int Pruner<FT>::gradient_descent_step(vec &b)
{
  int dn    = b.size();
  FT cf     = target_function(b);
  FT old_cf = cf;
  vec new_b(dn);
  vec gradient(dn);
  target_function_gradient(b, gradient);
  FT norm = 0.0;

  // normalize the gradient
  for (int i = 0; i < dn; ++i)
  {
    norm += gradient[i] * gradient[i];
    new_b[i] = b[i];
  }

  if (options.verbose)
  {
    log_stream() << "  Gradient descent step starts at cf=" << cf << '\n';
  }

  norm /= static_cast<double>(dn);
  norm = sqrt(norm);

  if (options.verbose)
  {
    log_stream() << "  Gradient norm " << norm << '\n';
  }

  if (norm <= 0.)
  {
    return 0;
  }

  for (int i = 0; i < dn; ++i)
  {
    gradient[i] /= norm;
  }
  FT new_cf;

  FT step = min_step;
  int j;

  for (j = 0;; ++j)
  {
    if (step > dn)
    {
      return -1;
    }

    for (int i = 0; i < dn; ++i)
    {
      new_b[i] = new_b[i] + step * gradient[i];
    }

    enforce(new_b);

    new_cf = target_function(new_b);

    if (new_cf >= cf)
    {
      break;
    }
    b  = new_b;
    cf = new_cf;
    step *= step_factor;
  }

  if (options.verbose)
  {
    log_stream() << "  Gradient descent step ends after " << j << " mini-steps at cf=" << cf
                 << '\n';
  }

  if (cf > old_cf * min_cf_decrease)
  {
    return 0;
  }
  return j;
}

/** Optimizes b with the Nelder-Mead method. Following the notation of
    https://en.wikipedia.org/wiki/Nelder%E2%80%93Mead_method */
template <class FT> int Pruner<FT>::nelder_mead_step(evec &b)
{
  int dn = b.size();
  int l  = dn + 1;
  std::vector<evec> bs(l);  // the simplex: (d+1) vectors of dimension d
  std::vector<FT> fs(l);    // values of f at the simplex vertices

  for (int i = 0; i < l; ++i)  // initialize the simplex
  {
    bs[i] = b;  // start from b
    if (i < dn)
    {
      bs[i][i] += (bs[i][i] < .5) ? detail::nd_init_width : -detail::nd_init_width;
    }
    enforce(bs[i]);
    fs[i] = target_function(bs[i]);
  }

  FT init_cf = fs[l - 1];

  vec bo(dn);  // centroid

  FT fs_maxi_last = fs[0];  // value of the last centroid

  if (options.verbose)
  {
    log_stream() << "  Starting nelder_mead cf = " << init_cf << " proba = " << measure_metric(b)
                 << '\n';
  }
  unsigned int counter = 0;
  int mini = 0, maxi = 0, maxi2 = 0;
  while (true)  // main loop
  {
    mini = maxi = maxi2 = 0;
    for (int i = 0; i < dn; ++i)
    {
      bo[i] = bs[0][i];
    }

    ////////////////
    // step 1. and 2. : order and centroid
    ////////////////
    for (int i = 1; i < l; ++i)  // determine min and max, and centroid
    {
      mini = (fs[i] < fs[mini]) ? i : mini;
      maxi = (fs[i] > fs[mini]) ? i : maxi;
      for (int j = 0; j < dn; ++j)
      {
        bo[j] += bs[i][j];
      }
    }
    FT tmp = l;
    for (int i = 0; i < dn; ++i)
    {
      bo[i] /= tmp;  // centroid calculated
    }

    fs_maxi_last = (!counter) ? fs[maxi] : fs_maxi_last;

    // determine min and max, and centroid
    maxi2 += (!maxi);
    for (int i = 1; i < l; ++i)
    {
      maxi2 = ((fs[i] > fs[maxi2]) && (i != maxi)) ? i : maxi2;
    }

    if (enforce(bo))  // maybe want to skip this test, that may be costly
    {
      FPLLL_ABORT("Pruner: concavity says that should not happen");
    }

    if (options.verbose)  // not sure such verbosity is of any use
    {
      log_stream() << "  melder_mead step " << counter << "cf = " << fs[mini]
                   << " proba = " << measure_metric(bs[mini])
                   << " cost = " << single_enum_cost(bs[mini]) << '\n';
      for (int i = 0; i < dn; ++i)
      {
        log_stream() << std::ceil(bs[mini][i].get_d() * 1000) << " ";
      }
      log_stream() << '\n';
    }

    ////////////////
    // stopping condition (not documented on wikipedia, improvising)
    ////////////////

    counter++;
    if (!(counter % l))  // every l steps, check progress and stop if none is done
    {
      if (fs[maxi] > fs_maxi_last * min_cf_decrease)
      {
        break;
      }
      fs_maxi_last = fs[maxi];
    }

    for (int i = 0; i < l; ++i)  // determine second best
    {
      if ((fs[i] > fs[maxi2]) && (i != maxi))
      {
        maxi2 = i;
      }
    }

    if (options.verbose)
    {
      log_stream() << mini << " " << maxi2 << " " << maxi << '\n';
      log_stream() << fs[mini] << " < " << fs[maxi2] << " < " << fs[maxi] << " | " << '\n';
    }

    ////////////////
    // step 3. reflection
    ////////////////
    vec br(dn);  // reflected point
    FT fr;       // value at the reflection point
    for (int i = 0; i < dn; ++i)
    {
      br[i] = bo[i] + detail::nd_alpha * (bo[i] - bs[maxi][i]);
    }
    enforce(br);
    fr = target_function(br);
    if (options.verbose)
    {
      log_stream() << "fr " << fr << '\n';
    }

    if ((fs[mini] <= fr) && (fr < fs[maxi2]))
    {
      bs[maxi] = br;
      fs[maxi] = fr;
      if (options.verbose)
      {
        log_stream() << "    Reflection " << '\n';
      }
      continue;  // go to step 1
    }

    ////////////////
    // step 4. expansion
    ////////////////
    if (fr < fs[mini])
    {
      vec be(dn);
      FT fe;
      for (int i = 0; i < dn; ++i)
      {
        be[i] = bo[i] + detail::nd_gamma * (br[i] - bo[i]);
      }
      enforce(be);
      fe = target_function(be);
      if (options.verbose)
      {
        log_stream() << "fe " << fe << '\n';
      }
      if (fe < fr)
      {
        bs[maxi] = be;
        fs[maxi] = fe;
        if (options.verbose)
        {
          log_stream() << "    Expansion A " << '\n';
        }
        continue;  // go to step 1
      }
      bs[maxi] = br;
      fs[maxi] = fr;
      if (options.verbose)
      {
        log_stream() << "    Expansion B " << '\n';
      }
      continue;  // go to step 1
    }

    ////////////////
    // step 5. contraction
    ////////////////
    if (!(fr >= fs[maxi2]))  // here, it is certain that fr >= fs[maxi2]
    {
      FPLLL_ABORT("Pruner: something certain is false in Nelder-Mead");
    }

    vec bc(dn);
    FT fc;
    for (int i = 0; i < dn; ++i)
    {
      bc[i] = bo[i] + detail::nd_rho * (bs[maxi][i] - bo[i]);
    }
    enforce(bc);
    fc = target_function(bc);
    if (options.verbose)
    {
      log_stream() << "fc " << fc << '\n';
    }
    if (fc < fs[maxi])
    {
      bs[maxi] = bc;
      fs[maxi] = fc;
      if (options.verbose)
      {
        log_stream() << "    Contraction " << '\n';
      }
      continue;  // go to step 1
    }

    ////////////////
    // step 6. shrink
    ////////////////
    if (options.verbose)
    {
      log_stream() << "    Shrink " << '\n';
    }
    for (int j = 0; j < l; ++j)
    {
      for (int i = 0; i < dn; ++i)
      {
        bs[j][i] = bs[mini][i] + detail::nd_sigma * (bs[j][i] - bs[mini][i]);
      }
      enforce(bs[j]);
      fs[j] = target_function(bs[j]);
    }
  }

  b            = bs[mini];
  int improved = (init_cf * min_cf_decrease) > fs[mini];

  if (options.verbose)
  {
    log_stream() << "Done nelder_mead, after " << counter << " steps" << '\n';
    log_stream() << "Final cf = " << fs[mini] << " proba = " << measure_metric(b) << '\n';
    if (improved)
    {
      log_stream() << "Progress has been made: init cf = " << init_cf << '\n';
    }
    log_stream() << '\n';
  }

  return improved;  // has Nelder-Mead made any progress
}

}  // namespace fplll
