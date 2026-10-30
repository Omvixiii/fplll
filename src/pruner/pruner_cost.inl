// SPDX-FileCopyrightText: 2015-2017 Martin Albrecht, Leo Ducas
//
// SPDX-License-Identifier: LGPL-2.1-or-later
/** \file pruner_cost.inl
    Enumeration-cost kernels of the pruner. */

namespace fplll
{

/** Single-enumeration cost using half-vector coefficients b. */
template <class FT>
FT Pruner<FT>::single_enum_cost_evec(const evec &b, std::vector<double> *detailed_cost)
{
  FPLLL_CHECK(shape_loaded, "Pruner: no basis shape was loaded");

  if (detailed_cost)
  {
    detailed_cost->resize(static_cast<std::size_t>(n));
  }
  vec rv(n);  // Relative volumes at each level

  /* even dimension of 2*i, e.g. 2, 4, ... */
  for (int i = 0; i < d; ++i)
  {
    rv[2 * i + 1] = relative_volume(i + 1, b);
  }

  /* odd dimension, e.g. get dim 3 by projecting (dim 2 and 4) */
  rv[0] = 1;
  for (int i = 1; i < d; ++i)
  {
    rv[2 * i] = sqrt(rv[2 * i - 1] * rv[2 * i + 1]);  // interpolate even values
  }

  FT total                 = 0.0;
  FT normalized_radius_pow = normalized_radius;

  for (int i = 0; i < 2 * d; ++i)
  {
    // ipv is normalized and larger
    // normalized_radius_pow = (R/det^1/n)^i and normalized to be
    // smaller, where R is the actual searching radius in SVP.
    FT tmp = normalized_radius_pow * rv[i] * tabulated_ball_vol[i + 1] *
             sqrt(pow_si(b[i / 2], 1 + i)) * ipv[i];

    tmp *= symmetry_factor;
    if (detailed_cost)
    {
      (*detailed_cost)[2 * d - (i + 1)] = tmp.get_d();
    }

    total += tmp;
    normalized_radius_pow *= normalized_radius;
  }
  if (!total.is_finite())
  {
    FPLLL_ABORT("Pruner: NaN or inf in single_enum_cost");
  }
  return total;
}

/** Lower bound of the single-enumeration cost. */
template <class FT>
FT Pruner<FT>::single_enum_cost_lower(const vec &b, std::vector<double> *detailed_cost)
{
  evec b_lower(d);
  for (int i = 0; i < d; ++i)
  {
    b_lower[i] = b[2 * i];
  }
  return single_enum_cost_evec(b_lower, detailed_cost);
}

/** Upper bound of the single-enumeration cost. */
template <class FT>
FT Pruner<FT>::single_enum_cost_upper(const vec &b, std::vector<double> *detailed_cost)
{
  evec b_upper(d);
  for (int i = 0; i < d; ++i)
  {
    b_upper[i] = b[2 * i + 1];
  }
  return single_enum_cost_evec(b_upper, detailed_cost);
}

/** Single-enumeration cost. */
template <class FT>
FT Pruner<FT>::single_enum_cost(const vec &b, std::vector<double> *detailed_cost)
{
  if (b.size() == static_cast<std::size_t>(d))
  {
    return single_enum_cost_evec(b, detailed_cost);
  }
  FT cl = single_enum_cost_lower(b, detailed_cost);
  FT cu = single_enum_cost_upper(b, detailed_cost);
  return (cl + cu) / 2.0;
}

template <class FT> void Pruner<FT>::target_function_gradient(const vec &b, vec &res)
{
  int dn = b.size();
  vec b_plus_db(dn);
  res[dn - 1] = 0.0;  // force null gradient on the last coordinate: don't touch this coefficient
  for (int i = 0; i < dn - 1; ++i)
  {
    b_plus_db = b;
    b_plus_db[i] *= (1.0 - epsilon);
    enforce(b_plus_db, i);
    FT X = target_function(b_plus_db);

    b_plus_db = b;
    b_plus_db[i] *= (1.0 + epsilon);
    enforce(b_plus_db, i);
    FT Y   = target_function(b_plus_db);
    res[i] = (log(X) - log(Y)) / epsilon;
  }
}

template <class FT> FT Pruner<FT>::target_function(const evec &b)
{
  if (metric == PrunerMetric::probability_of_shortest)
  {
    FT probability = svp_probability(b);
    FT trials      = log(1.0 - target) / log(1.0 - probability);
    if (!trials.is_finite())
    {
      FPLLL_ABORT("Pruner: NaN or inf in target_function (probability metric); a higher precision "
                  "sometimes helps");
    }
    trials = trials < 1.0 ? 1.0 : trials;
    return single_enum_cost(b) * trials + preproc_cost * (trials - 1.0);
  }

  FT expected = expected_solutions(b);
  FT trials   = target / expected;
  if (!trials.is_finite())
  {
    FPLLL_ABORT("Pruner: NaN or inf in target_function (expected-solutions metric); a higher "
                "precision sometimes helps");
  }
  // if expected solutions > 1, set trials = 1
  trials = trials < 1.0 ? 1.0 : trials;
  return single_enum_cost(b) * trials + preproc_cost * (trials - 1.0);
}

}  // namespace fplll
