// SPDX-FileCopyrightText: 2016-2017 Leo Ducas
// SPDX-FileCopyrightText: 2018 Shi Bai
//
// SPDX-License-Identifier: LGPL-2.1-or-later
/** \file pruner_optimize_tp.inl
    Optimization of the success probability. */

namespace fplll
{

namespace detail
{
inline constexpr double optimize_prob_min_step = 1e-4;
inline constexpr double optimize_prob_max_step = 1e4;
}  // namespace detail

template <class FT> void Pruner<FT>::optimize_coefficients_incr_prob(std::vector<double> &pr)
{
  int dn = static_cast<int>(pr.size());
  double normalized;
  FT old_prob;
  vec b(dn), old_b(dn);
  std::vector<double> detailed_cost(dn);
  std::vector<double> weight(dn);
  bool not_changed;

  load_coefficients(b, pr);

  // increase b until the target is achieved, or quit if impossible
  int tours = 0;
  while (true)
  {
    if (tours > detail::optimize_prob_max_step)
    {
      break;
    }
    tours++;

    old_prob = measure_metric(b);
    if (old_prob >= target)
    {
      break;
    }

    single_enum_cost(b, &detailed_cost);
    normalized = 0.0;
    for (int i = 0; i < dn; i++)
    {
      weight[i] = 0.0;
      for (int j = i; j < dn; j++)
      {
        weight[i] = weight[i] + detailed_cost[j];
      }
      weight[i] = 1.0 / weight[i];
      if (weight[i] < detail::optimize_prob_min_step)
      {
        weight[i] = detail::optimize_prob_min_step;
      }
      normalized += weight[i];
    }
    for (int i = 0; i < dn; i++)
    {
      weight[i] = weight[i] / normalized;
    }
    for (int i = dn - 1; i >= 0; --i)
    {
      old_b[i] = b[i];
      b[i]     = b[i] + weight[i];
      if (b[i] >= 1.0)
      {
        b[i] = 1.0;
      }
    }

    enforce(b);

    not_changed = true;
    for (int i = dn - 1; i >= 0; --i)
    {
      if (b[i] != old_b[i])
      {
        not_changed = false;
      }
    }
    if (not_changed)
    {
      break;
    }
  }

  save_coefficients(pr, b);
}

template <class FT> void Pruner<FT>::optimize_coefficients_decr_prob(std::vector<double> &pr)
{
  int dn = static_cast<int>(pr.size());
  double normalized;
  FT old_prob;
  vec b(dn), old_b(dn);
  std::vector<double> detailed_cost(dn);
  std::vector<double> weight(dn);
  bool not_changed;

  load_coefficients(b, pr);

  // decrease b until the target is achieved
  int tours = 0;
  while (true)
  {
    if (tours > detail::optimize_prob_max_step)
    {
      break;
    }
    tours++;

    old_prob = measure_metric(b);
    if (old_prob <= target)
    {
      break;
    }

    single_enum_cost(b, &detailed_cost);
    normalized = 0.0;
    for (int i = 0; i < dn; i++)
    {
      weight[i] = 0.0;
      for (int j = i; j < dn; j++)
      {
        weight[i] = weight[i] + detailed_cost[j];
      }
      weight[i] = 1.0 / weight[i];
      if (weight[i] < detail::optimize_prob_min_step)
      {
        weight[i] = detail::optimize_prob_min_step;
      }
      normalized += weight[i];
    }
    for (int i = 0; i < dn; i++)
    {
      weight[i] = weight[i] / normalized;
    }

    for (int i = dn - 1; i >= 0; --i)
    {
      old_b[i] = b[i];
      b[i]     = b[i] - weight[i];
      if (b[i] < detail::optimize_prob_min_step)
      {
        b[i] = detail::optimize_prob_min_step;
      }
    }

    enforce(b);

    not_changed = true;
    for (int i = dn - 1; i >= 0; --i)
    {
      if (b[i] != old_b[i])
      {
        not_changed = false;
      }
    }
    if (not_changed)
    {
      break;
    }
  }
  save_coefficients(pr, b);
}

template <class FT>
void Pruner<FT>::optimize_coefficients_local_adjust_prob(std::vector<double> &pr)
{
  int dn = static_cast<int>(pr.size());
  FT prob, ratio;
  vec b(dn), old_b(dn);
  bool not_changed;

  load_coefficients(b, pr);

  // increase b until the target is achieved
  while (true)
  {
    prob  = measure_metric(b);
    ratio = prob / target;

    // good enough
    if (ratio < 1.05 && ratio > 0.95)
    {
      break;
    }

    // tune
    if (ratio < 1)
    {
      for (int i = dn - 1; i >= 0; --i)
      {
        old_b[i] = b[i];
        b[i]     = b[i] + detail::optimize_prob_min_step;
        if (b[i] >= 1.0)
        {
          b[i] = 1.0;
        }
      }
    }
    else
    {
      for (int i = dn - 1; i >= 0; --i)
      {
        old_b[i] = b[i];
        b[i]     = b[i] - detail::optimize_prob_min_step;
        if (b[i] < detail::optimize_prob_min_step)
        {
          b[i] = detail::optimize_prob_min_step;
        }
      }
    }

    enforce(b);

    not_changed = true;
    for (int i = dn - 1; i >= 0; --i)
    {
      if (b[i] != old_b[i])
      {
        not_changed = false;
      }
    }
    if (not_changed)
    {
      break;
    }
  }

  save_coefficients(pr, b);
}

}  // namespace fplll
