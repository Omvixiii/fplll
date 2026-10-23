
#include "bkz_param.h"
#include "nr/nr.h"
#include <cstdio>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

FPLLL_BEGIN_NAMESPACE

PruningParams PruningParams::LinearPruningParams(int block_size, int level)
{

  PruningParams pruning = PruningParams();
  int start_descent     = block_size - level;

  if (start_descent > block_size)
    start_descent = block_size;

  if (start_descent < 1)
    start_descent = 1;

  pruning.coefficients.resize(block_size);
  for (int k = 0; k < start_descent; k++)
  {
    pruning.coefficients[k] = 1.0;
  }
  for (int k = 0; k < block_size - start_descent; k++)
  {
    pruning.coefficients[start_descent + k] = ((double)(block_size - k - 1)) / block_size;
  }
  pruning.gh_factor   = 1.0;
  pruning.metric      = PRUNER_METRIC_PROBABILITY_OF_SHORTEST;
  pruning.expectation = fplll::svp_probability<FP_NR<double>>(pruning.coefficients).get_d();

  return pruning;
}

const std::string &default_strategy_path()
{
  static const std::string ret(FPLLL_DEFAULT_STRATEGY_PATH);
  return ret;
}

std::optional<std::string> strategy_full_path(std::string_view strategy_path)
{
  const std::string candidate(strategy_path);
  if (std::ifstream(candidate).good())
    return candidate;
  std::string path = default_strategy_path() + "/" + candidate;
  if (std::ifstream(path).good())
    return path;
  return std::nullopt;
}

const std::string &default_strategy()
{
  static const std::string ret(FPLLL_DEFAULT_STRATEGY);
  return ret;
}

const PruningParams &Strategy::get_pruning(double radius, double gh) const
{
  static const PruningParams fallback;
  if (pruning_parameters.empty())
    return fallback;

  double gh_factor    = radius / gh;
  double closest_dist = pow(2, 80);
  auto best           = pruning_parameters.begin();

  for (auto it = pruning_parameters.begin(); it != pruning_parameters.end(); ++it)
  {
    if (fabs(it->gh_factor - gh_factor) < closest_dist)
    {
      closest_dist = fabs(it->gh_factor - gh_factor);
      best         = it;
    }
  }

  return *best;
}

vector<Strategy> load_strategies_json(std::string_view filename)
{
  json js;
  {
    const std::optional<std::string> full_path = strategy_full_path(filename);
    if (!full_path)
      throw std::runtime_error("Cannot open strategies file '" + std::string(filename) +
                               "'. Tried '" + std::string(filename) + "' and '" +
                               default_strategy_path() + "/" + std::string(filename) +
                               "'. Configured strategy directory: '" + default_strategy_path() +
                               "'. Install the strategy data at the configured prefix or pass an "
                               "existing strategy file explicitly.");
    std::ifstream fs(*full_path);
    fs >> js;
  }

  if (!js.is_array())
    throw std::invalid_argument("strategy file '" + std::string(filename) +
                                "' must contain a JSON array");

  vector<Strategy> strategies;

  for (auto it = js.begin(); it != js.end(); ++it)
  {
    const json &j_strat = *it;
    if (!j_strat.is_object())
      throw std::invalid_argument("strategy entries must be JSON objects");

    const auto block_it = j_strat.find("block_size");
    if (block_it == j_strat.end() || !block_it->is_number_integer())
      throw std::invalid_argument("strategy entry without an integer 'block_size'");
    const long long block_size_signed = block_it->get<long long>();
    if (block_size_signed < 0 || block_size_signed > 4096)
      throw std::invalid_argument("strategy block_size out of range (0..4096)");
    const size_t block_size = static_cast<size_t>(block_size_signed);

    // ensure to add this strategy in the right place
    while (strategies.size() <= block_size)
    {
      strategies.emplace_back();
    }

    Strategy strategy;
    strategy.block_size = block_size;

    if (j_strat.find("preprocessing_block_sizes") != j_strat.end())
    {
      const json &preprocessing = j_strat["preprocessing_block_sizes"];
      if (!preprocessing.is_array())
        throw std::invalid_argument("'preprocessing_block_sizes' must be an array");
      for (auto p_it = preprocessing.begin(); p_it != preprocessing.end(); ++p_it)
      {
        long long pre_block = -1;
        if (p_it->is_number_integer())
        {
          pre_block = p_it->get<long long>();
        }
        else if (p_it->is_object())
        {
          const auto pre_it = p_it->find("block_size");
          if (pre_it == p_it->end() || !pre_it->is_number_integer())
            throw std::invalid_argument(
                "'preprocessing_block_sizes' entry without an integer block_size");
          pre_block = pre_it->get<long long>();
        }
        else
        {
          throw std::invalid_argument("invalid 'preprocessing_block_sizes' entry");
        }
        if (pre_block < 0 || pre_block > 4096)
          throw std::invalid_argument("preprocessing block size out of range (0..4096)");
        strategy.preprocessing_block_sizes.emplace_back(static_cast<size_t>(pre_block));
      }
    }

    if (j_strat.find("pruning_parameters") != j_strat.end())
    {
      const json &pruning_parameters = j_strat["pruning_parameters"];
      if (!pruning_parameters.is_array())
        throw std::invalid_argument("'pruning_parameters' must be an array");
      for (auto p_it = pruning_parameters.begin(); p_it != pruning_parameters.end(); ++p_it)
      {
        const json &j_prun = *p_it;
        if (!j_prun.is_array() || j_prun.size() < 3 || !j_prun[0].is_number() ||
            !j_prun[1].is_array() || !j_prun[2].is_number())
        {
          throw std::invalid_argument(
              "pruning entries must start with [gh_factor, coefficients, expectation]");
        }
        PruningParams pruning;
        pruning.gh_factor = j_prun[0].get<double>();

        for (auto c_it = j_prun[1].begin(); c_it != j_prun[1].end(); ++c_it)
        {
          if (!c_it->is_number())
            throw std::invalid_argument("pruning coefficients must be numbers");
          pruning.coefficients.emplace_back(c_it->get<double>());
        }
        pruning.expectation = j_prun[2].get<double>();
        if (!(pruning.expectation > 0.0 && pruning.expectation <= 1.0))
          throw std::invalid_argument("pruning expectation must be in (0, 1]");
        // TODO: don't hardcode success probability as metric
        pruning.metric = PRUNER_METRIC_PROBABILITY_OF_SHORTEST;

        strategy.pruning_parameters.emplace_back(pruning);
      }
    }

    strategies[block_size] = std::move(strategy);
  }

  // finally, we make sure all strategies are sound
  for (auto it = strategies.begin(); it != strategies.end(); ++it)
  {
    if (it->pruning_parameters.size() == 0)
      it->pruning_parameters.emplace_back(PruningParams());
  }

  return strategies;
}

FPLLL_END_NAMESPACE
