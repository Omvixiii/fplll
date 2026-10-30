// SPDX-FileCopyrightText: 2015-2017 Martin Albrecht, Leo Ducas
// SPDX-FileCopyrightText: 2018 Shi Bai
//
// SPDX-License-Identifier: LGPL-2.1-or-later
/** \file pruner_util.inl
    Basis-shape loading and coefficient conversion of the pruner. */

namespace fplll
{

template <class FT>
Pruner<FT>::Pruner(const int dimension, const PrunerOptions &options)
    : options(options), n(dimension)
{
  initialize();
}

template <class FT>
Pruner<FT>::Pruner(const FT &enumeration_radius, const FT &preproc_cost,
                   const std::vector<double> &gso_r, const FT &target, PrunerMetric metric,
                   const PrunerOptions &options)
    : enumeration_radius(enumeration_radius), preproc_cost(preproc_cost), target(target),
      metric(metric), options(options), n(static_cast<int>(gso_r.size()))
{
  initialize();
  load_basis_shape(gso_r);
}

template <class FT>
Pruner<FT>::Pruner(const FT &enumeration_radius, const FT &preproc_cost,
                   const std::vector<std::vector<double>> &gso_rs, const FT &target,
                   PrunerMetric metric, const PrunerOptions &options)
    : enumeration_radius(enumeration_radius), preproc_cost(preproc_cost), target(target),
      metric(metric), options(options)
{
  FPLLL_CHECK(!gso_rs.empty(), "Pruner: at least one basis shape is required");
  n = static_cast<int>(gso_rs[0].size());
  initialize();
  load_basis_shapes(gso_rs);
}

template <class FT> void Pruner<FT>::initialize()
{
  constexpr int max_dim = static_cast<int>(max_pruner_dim);
  // The tables are halved to d = n / 2 and the expected-solutions formula
  // indexes ipv[2 * d - 1]: n < 2 is a precondition violation, not a
  // degenerate input (it used to read out of bounds / divide by zero).
  // Validate before building the tabulated constants (their first
  // construction is the expensive part).
  FPLLL_CHECK(n >= 2 && n < max_dim, "Pruner: dimension {} is outside [2, {})", n, max_pruner_dim);
  set_tabulated_consts();
  d          = n / 2;
  opt_single = options.single;

  if (options.cvp)
  {
    symmetry_factor = 1.;
  }
  if (options.single)
  {
    FPLLL_CHECK(!options.half, "Pruner: the half and single options are mutually exclusive");
  }

  if (metric == PrunerMetric::probability_of_shortest)
  {
    FPLLL_CHECK(target > 0.0 && target < 1.0,
                "Pruner: the target must satisfy 0 < target < 1 for the probability metric");
  }
  else
  {
    FPLLL_CHECK(target > 0.0,
                "Pruner: the target must be positive for the expected-solutions metric");
  }

  min_pruning_coefficients.assign(static_cast<std::size_t>(d), 0.);
}

template <class FT>
void Pruner<FT>::load_basis_shape(const std::vector<double> &gso_r, bool reset_normalization)
{
  shape_loaded = true;
  FT tmp;
  logvol = 0.0;
  r.resize(n);
  ipv.resize(n);
  for (int i = 0; i < n; ++i)
  {
    r[i] = gso_r[n - 1 - i];
    logvol += log(r[i]);
  }

  // by default reset
  if (reset_normalization)
  {
    /* just (1 / det^(1/n))^2, the squared normalization factor */
    normalization_factor = exp(logvol / static_cast<float>(-n));

    /* re-normalise R = radius_d * (1 / det^(1/n)) */
    normalized_radius = sqrt(enumeration_radius * normalization_factor);
  }

  // r[i] is normalized by (1 / det^(1/n))^2
  for (int i = 0; i < n; ++i)
  {
    r[i] *= normalization_factor;
  }

  tmp = 1.;
  // hence ipv is also normalized where ipv[n-1] = 1
  for (int i = 0; i < 2 * d; ++i)
  {
    tmp *= sqrt(r[i]);
    ipv[i] = 1.0 / tmp;
  }
}

template <class FT>
void Pruner<FT>::load_basis_shapes(const std::vector<std::vector<double>> &gso_rs)
{
  vec sum_ipv(n);
  std::fill(sum_ipv.begin(), sum_ipv.end(), 0.);

  const int count = static_cast<int>(gso_rs.size());
  for (int k = 0; k < count; ++k)
  {
    FPLLL_CHECK(gso_rs[k].size() == static_cast<std::size_t>(n),
                "Pruner: loading several bases with different dimensions");
    load_basis_shape(gso_rs[k], k == 0);
    for (int i = 0; i < n; ++i)
    {
      sum_ipv[i] += ipv[i];
    }
  }
  for (int i = 0; i < n; ++i)
  {
    ipv[i] = sum_ipv[i] / (1.0 * count);
  }
}

template <class FT> FT Pruner<FT>::gaussian_heuristic()
{
  return exp(2. * log(tabulated_ball_vol[n]) / static_cast<float>(-n)) / normalization_factor;
}

template <class FT>
void Pruner<FT>::save_coefficients(/*o*/ std::vector<double> &pr, /*i*/ const evec &b)
{
  pr.resize(n);
  int dn = b.size();
  if (static_cast<std::size_t>(dn) == static_cast<std::size_t>(d))
  {
    for (int i = 0; i < d; ++i)
    {
      pr[n - 1 - 2 * i] = b[i].get_d();
      pr[n - 2 - 2 * i] = b[i].get_d();
    }
  }
  else
  {
    for (int i = 0; i < n; ++i)
    {
      pr[n - 1 - i] = b[i].get_d();
    }
  }
  pr[0] = 1.;
}

/** Loads pruning coefficients. */
template <class FT>
void Pruner<FT>::load_coefficients(/*o*/ vec &b, /*i*/ const std::vector<double> &pr)
{
  int dn = b.size();
  int c  = (static_cast<std::size_t>(dn) == static_cast<std::size_t>(d)) ? 2 : 1;
  for (int i = 0; i < dn; ++i)
  {
    b[i] = pr[n - c * i - 1];
  }
}

template <class FT>
double Pruner<FT>::single_enum_cost(const std::vector<double> &pr,
                                    std::vector<double> *detailed_cost)
{
  evec b(d);
  load_coefficients(b, pr);
  return single_enum_cost(b, detailed_cost).get_d();
}

template <class FT> double Pruner<FT>::measure_metric(const std::vector<double> &pr)
{
  vec b(n);
  load_coefficients(b, pr);
  return measure_metric(b).get_d();
}

}  // namespace fplll
