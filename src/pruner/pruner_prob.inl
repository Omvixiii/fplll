// SPDX-FileCopyrightText: 2015-2017 Martin Albrecht, Leo Ducas
//
// SPDX-License-Identifier: LGPL-2.1-or-later
/** \file pruner_prob.inl
    Success-probability and expected-solutions kernels of the pruner. */

namespace fplll
{

template <class FT> FT Pruner<FT>::svp_probability_evec(const evec &b)
{
  evec b_minus_db(d);
  FT dx = shell_ratio;
  for (int i = 0; i < d; ++i)
  {
    b_minus_db[i] = b[i] / (dx * dx);
    if (b_minus_db[i] > 1)
    {
      b_minus_db[i] = 1;
    }
  }

  FT vol  = relative_volume(d, b);
  FT dxn  = pow_si(dx, 2 * d);
  FT dvol = dxn * relative_volume(d, b_minus_db) - vol;
  FT res  = dvol / (dxn - 1.);

  if (!res.is_finite())
  {
    FPLLL_ABORT("Pruner: NaN or inf in svp_probability");
  }
  return res;
}

/** Hopefully a lower bound for bounding b. */
template <class FT> FT Pruner<FT>::svp_probability_lower(const vec &b)
{
  evec b_lower(d);
  for (int i = 0; i < d; ++i)
  {
    b_lower[i] = b[2 * i];
  }
  return svp_probability_evec(b_lower);
}

/** Upper bound for bounding b. */
template <class FT> FT Pruner<FT>::svp_probability_upper(const vec &b)
{
  evec b_upper(d);
  for (int i = 0; i < d; ++i)
  {
    b_upper[i] = b[2 * i + 1];
  }
  return svp_probability_evec(b_upper);
}

/** Upper estimate for bounding b. */
template <class FT> FT Pruner<FT>::svp_probability(const vec &b)
{
  if (b.size() == static_cast<std::size_t>(d))
  {
    return svp_probability_evec(b);
  }
  FT pl = svp_probability_lower(b);
  FT pu = svp_probability_upper(b);
  return (pl + pu) / 2.0;
}

template <class FT> FT Pruner<FT>::expected_solutions_evec(const evec &b)
{
  int j  = d * 2 - 1;
  FT tmp = log(relative_volume(d, b));
  tmp += log(tabulated_ball_vol[j + 1]);
  tmp += (log(normalized_radius) + log(b[j / 2]) / 2.0) * (j + 1);
  tmp += log(ipv[j]);
  tmp += log(symmetry_factor);
  tmp = exp(tmp);

  if (!tmp.is_finite())
  {
    FPLLL_ABORT("Pruner: NaN or inf in expected_solutions");
  }
  return tmp;
}

template <class FT> FT Pruner<FT>::expected_solutions_lower(const vec &b)
{
  evec b_lower(d);
  for (int i = 0; i < d; ++i)
  {
    b_lower[i] = b[2 * i];
  }
  return expected_solutions_evec(b_lower);
}

template <class FT> FT Pruner<FT>::expected_solutions_upper(const vec &b)
{
  evec b_upper(d);
  for (int i = 0; i < d; ++i)
  {
    b_upper[i] = b[2 * i + 1];
  }
  return expected_solutions_evec(b_upper);
}

template <class FT> FT Pruner<FT>::expected_solutions(const vec &b)
{
  FPLLL_CHECK(shape_loaded, "Pruner: no basis shape was loaded");
  if (b.size() == static_cast<std::size_t>(d))
  {
    return expected_solutions_evec(b);
  }
  FT pl = expected_solutions_lower(b);
  FT pu = expected_solutions_upper(b);
  return (pl + pu) / 2.0;
}

template <class FT> FT Pruner<FT>::measure_metric(const vec &b)
{
  if (metric == PrunerMetric::probability_of_shortest)
  {
    return svp_probability(b);
  }
  return expected_solutions(b);
}

}  // namespace fplll
