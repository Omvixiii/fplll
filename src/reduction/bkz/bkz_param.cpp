// SPDX-FileCopyrightText: 2016 Guillaume Bonnoron
// SPDX-FileCopyrightText: 2016-2017 Leo Ducas
// SPDX-FileCopyrightText: 2016-2017 Martin R. Albrecht
// SPDX-FileCopyrightText: 2016 Tancrède Lepoint
// SPDX-FileCopyrightText: 2022 Joe Rowell
// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <fplll/defs.h>
#include <fplll/pruner/pruning_params.h>
#include <fplll/reduction/bkz_param.h>

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstddef>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fplll
{

namespace
{

/** Resolves a strategy file name: first as given, then in the compiled-in
    strategy directory.  Returns nullopt when neither exists. */
std::optional<std::string> find_strategy_file(std::string_view filename)
{
  std::string candidate(filename);
  if (std::ifstream(candidate).good())
  {
    return candidate;
  }
  candidate = std::string(FPLLL_STRATEGIES_DIR) + "/" + std::string(filename);
  if (std::ifstream(candidate).good())
  {
    return candidate;
  }
  return std::nullopt;
}

}  // namespace

const PruningParams &Strategy::get_pruning(double radius, double gh) const
{
  static const PruningParams fallback;
  if (pruning_parameters.empty())
  {
    return fallback;
  }

  const double gh_factor = radius / gh;
  double closest_dist    = std::pow(2.0, 80.0);
  auto best              = pruning_parameters.begin();

  for (auto it = pruning_parameters.begin(); it != pruning_parameters.end(); ++it)
  {
    if (std::fabs(it->gh_factor - gh_factor) < closest_dist)
    {
      closest_dist = std::fabs(it->gh_factor - gh_factor);
      best         = it;
    }
  }

  return *best;
}

std::vector<Strategy> load_strategies_json(std::string_view filename)
{
  using nlohmann::json;

  const std::optional<std::string> full_path = find_strategy_file(filename);
  FPLLL_CHECK(full_path.has_value(),
              "cannot open BKZ strategy file '{}': tried '{}' and '" FPLLL_STRATEGIES_DIR "/{}'",
              filename, filename, filename);

  std::vector<Strategy> strategies;
  try
  {
    json js;
    {
      std::ifstream fs(*full_path);
      FPLLL_CHECK(fs.good(), "cannot open BKZ strategy file '{}'", *full_path);
      fs >> js;
    }

    FPLLL_CHECK(js.is_array(), "BKZ strategy file '{}' must contain a JSON array", *full_path);

    for (const auto &j_strat : js)
    {
      FPLLL_CHECK(j_strat.is_object(), "BKZ strategy entries must be JSON objects (file '{}')",
                  *full_path);

      const auto block_it = j_strat.find("block_size");
      FPLLL_CHECK(block_it != j_strat.end() && block_it->is_number_integer(),
                  "BKZ strategy entry without an integer 'block_size' (file '{}')", *full_path);
      const long long block_size_signed = block_it->get<long long>();
      FPLLL_CHECK(block_size_signed >= 0 && block_size_signed <= 4096,
                  "BKZ strategy block_size out of range 0..4096 (file '{}')", *full_path);
      const auto block_size = static_cast<std::size_t>(block_size_signed);

      // ensure to add this strategy in the right place
      while (strategies.size() <= block_size)
      {
        strategies.emplace_back();
      }

      Strategy strategy;
      strategy.block_size = block_size;

      if (const auto preprocessing_it = j_strat.find("preprocessing_block_sizes");
          preprocessing_it != j_strat.end())
      {
        const json &preprocessing = *preprocessing_it;
        FPLLL_CHECK(preprocessing.is_array(),
                    "'preprocessing_block_sizes' must be an array (file '{}')", *full_path);
        for (const auto &p_it : preprocessing)
        {
          long long pre_block = -1;
          if (p_it.is_number_integer())
          {
            pre_block = p_it.get<long long>();
          }
          else if (p_it.is_object())
          {
            const auto pre_it = p_it.find("block_size");
            FPLLL_CHECK(pre_it != p_it.end() && pre_it->is_number_integer(),
                        "'preprocessing_block_sizes' entry without an integer block_size "
                        "(file '{}')",
                        *full_path);
            pre_block = pre_it->get<long long>();
          }
          else
          {
            FPLLL_ABORT("invalid 'preprocessing_block_sizes' entry (file '{}')", *full_path);
          }
          FPLLL_CHECK(pre_block >= 0 && pre_block <= 4096,
                      "preprocessing block size out of range 0..4096 (file '{}')", *full_path);
          strategy.preprocessing_block_sizes.emplace_back(static_cast<std::size_t>(pre_block));
        }
      }

      if (const auto pruning_it = j_strat.find("pruning_parameters"); pruning_it != j_strat.end())
      {
        const json &pruning_parameters = *pruning_it;
        FPLLL_CHECK(pruning_parameters.is_array(),
                    "'pruning_parameters' must be an array (file '{}')", *full_path);
        for (const auto &j_prun : pruning_parameters)
        {
          FPLLL_CHECK(j_prun.is_array() && j_prun.size() >= 3 && j_prun[0].is_number() &&
                          j_prun[1].is_array() && j_prun[2].is_number(),
                      "pruning entries must start with [gh_factor, coefficients, expectation] "
                      "(file '{}')",
                      *full_path);
          PruningParams pruning;
          pruning.gh_factor = j_prun[0].get<double>();

          for (const auto &c_it : j_prun[1])
          {
            FPLLL_CHECK(c_it.is_number(), "pruning coefficients must be numbers (file '{}')",
                        *full_path);
            pruning.coefficients.emplace_back(c_it.get<double>());
          }
          pruning.expectation = j_prun[2].get<double>();
          FPLLL_CHECK(pruning.expectation > 0.0 && pruning.expectation <= 1.0,
                      "pruning expectation must be in (0, 1] (file '{}')", *full_path);
          pruning.metric = PrunerMetric::probability_of_shortest;

          strategy.pruning_parameters.emplace_back(std::move(pruning));
        }
      }

      strategies[block_size] = std::move(strategy);
    }

    // finally, we make sure all strategies are sound
    for (auto &strategy : strategies)
    {
      if (strategy.pruning_parameters.empty())
      {
        strategy.pruning_parameters.emplace_back();
      }
    }
  }
  catch (const nlohmann::json::exception &e)
  {
    FPLLL_ABORT("invalid BKZ strategy file '{}': {}", *full_path, e.what());
  }

  return strategies;
}

}  // namespace fplll
