// SPDX-FileCopyrightText: 2005-2008 Damien Stehle
// SPDX-FileCopyrightText: 2007 David Cade
// SPDX-FileCopyrightText: 2008-2011 Xavier Pujol
// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "cli.h"

#include <fplll/fplll.h>
#include <fplll/fplll_config.h>
#include <fplll/nr/matrix.h>
#include <fplll/nr/nr.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <format>
#include <fstream>
#include <gmp.h>
#include <iostream>
#include <optional>
#include <print>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

using namespace std::string_view_literals;

using fplll::BKZParam;
using fplll::BKZVariant;
using fplll::CVPOptions;
using fplll::FloatType;
using fplll::HKZOptions;
using fplll::LLLMethod;
using fplll::LLLOptions;
using fplll::LLLParameters;
using fplll::RedStatus;
using fplll::ReductionReport;
using fplll::SVPMethod;
using fplll::SVPOptions;

enum class Action : std::uint8_t
{
  lll,
  hlll,
  bkz,
  hkz,
  svp,
  cvp,
  pruner,
};

struct Options
{
  Action action = Action::lll;

  // LLL / HLLL
  double delta         = fplll::LLL_DEF_DELTA;
  double eta           = fplll::LLL_DEF_ETA;
  double theta         = fplll::HLLL_DEF_THETA;
  double hlll_c        = fplll::HLLL_DEF_C;
  LLLMethod method     = LLLMethod::wrapper;
  FloatType float_type = FloatType::automatic;
  int precision        = 0;
  bool verbose         = false;
  bool early_reduction = false;
  bool siegel          = false;
  bool no_lll          = false;

  // BKZ / HKZ
  int block_size     = 0;
  BKZVariant variant = BKZVariant::bkz;
  std::optional<int> max_loops;
  std::optional<std::chrono::milliseconds> max_time;
  std::optional<double> gh_factor;
  bool auto_abort  = false;
  bool bounded_lll = false;
  std::string strategies_file;

  // Pruner
  int prune_start = 0;
  std::optional<int> prune_end;
  double prune_pre_nodes = 1e6;
  std::optional<double> prune_min_probability;

  // I/O
  std::string output_format;
  std::optional<std::string> input_file;
};

/** Upper bound of `--bkz-max-time`, in seconds: about 31.7 years.  Larger
    values only mean "effectively no time limit"; keeping the bound far below
    `INT64_MAX / 1000` also makes the conversion to milliseconds exact. */
constexpr double max_bkz_max_time_seconds = 1e9;

/** Maps between the command-line spellings and the enumerations the library
    uses; one table serves parsing and help/error text. */
template <class E, std::size_t N> struct EnumNames
{
  std::array<std::pair<std::string_view, E>, N> entries;
  std::string_view label;

  [[nodiscard]] E parse(std::string_view text) const
  {
    const auto entry = std::ranges::find(entries, text, &std::pair<std::string_view, E>::first);
    if (entry == entries.end())
    {
      throw std::invalid_argument(std::format("unknown {} '{}'", label, text));
    }
    return entry->second;
  }
};

/** Convenience factory so the tables below read as a plain list of
    "spelling, enumerator" pairs. */
template <class E, std::size_t N>
constexpr EnumNames<E, N> make_enum_names(std::string_view label,
                                          std::array<std::pair<std::string_view, E>, N> entries)
{
  return EnumNames<E, N>{.entries = entries, .label = label};
}

inline constexpr auto action_name_entries = std::array{
    std::pair{"lll"sv, Action::lll},    std::pair{"hlll"sv, Action::hlll},
    std::pair{"bkz"sv, Action::bkz},    std::pair{"hkz"sv, Action::hkz},
    std::pair{"svp"sv, Action::svp},    std::pair{"cvp"sv, Action::cvp},
    std::pair{"pru"sv, Action::pruner},
};

inline constexpr auto action_names = make_enum_names("action"sv, action_name_entries);

inline constexpr auto lll_method_name_entries = std::array{
    std::pair{"wrapper"sv, LLLMethod::wrapper},
    std::pair{"proved"sv, LLLMethod::proved},
    std::pair{"heuristic"sv, LLLMethod::heuristic},
    std::pair{"fast"sv, LLLMethod::fast},
};

inline constexpr auto lll_method_names = make_enum_names("LLL method"sv, lll_method_name_entries);

inline constexpr auto float_type_name_entries = std::array{
    std::pair{"automatic"sv, FloatType::automatic},
    std::pair{"double"sv, FloatType::double_precision},
    std::pair{"dd"sv, FloatType::dd},
    std::pair{"qd"sv, FloatType::qd},
    std::pair{"mpfr"sv, FloatType::mpfr},
};

inline constexpr auto float_type_names =
    make_enum_names("floating-point type"sv, float_type_name_entries);

void print_help(const char *program)
{
  std::println("Usage: {} [options] [file]\n"
               "\n"
               "Reads a matrix (one basis vector per row) from file or stdin and runs\n"
               "the selected lattice algorithm on it.\n"
               "\n"
               "Actions:\n"
               "  -a, --action <lll|hlll|bkz|sdb|sld|hkz|svp|cvp|pru>\n"
               "                           algorithm to run (default: lll); sdb and sld\n"
               "                           are the self-dual and slide-reduction BKZ variants\n"
               "\n"
               "Reduction parameters:\n"
               "  -d, --delta <delta>       LLL delta (default: {})\n"
               "  -e, --eta <eta>           LLL eta (default: {})\n"
               "  -t, --theta <theta>       HLLL theta (default: {})\n"
               "  -c, --hlll-c <c>          HLLL constant c (default: {})\n"
               "  -m, --lll-method <wrapper|proved|heuristic|fast>\n"
               "                           LLL/HLLL variant (default: wrapper)\n"
               "  -f, --float-type <automatic|double|dd|qd|mpfr>\n"
               "                           floating-point backend (default: automatic)\n"
               "  -p, --precision <bits>    MPFR precision (only with --float-type mpfr)\n"
               "  -y, --early-reduction     enable early reduction\n"
               "      --siegel              use Siegel's condition instead of Lovasz's\n"
               "      --no-lll              skip the initial LLL reduction\n"
               "  -v, --verbose             print reduction diagnostics to stderr\n"
               "\n"
               "BKZ / HKZ parameters:\n"
               "  -b, --block-size <size>   block size (required for bkz and pru)\n"
               "      --bkz-max-loops <n>   stop after n tours\n"
               "      --bkz-max-time <s>    stop after s seconds of process CPU time\n"
               "                           (at most 1e9 seconds, about 31.7 years)\n"
               "      --bkz-auto-abort      stop when the slope stops decreasing\n"
               "      --bkz-bounded-lll     restrict LLL to the current block\n"
               "      --bkz-gh-factor <f>   multiply the Gaussian heuristic by f\n"
               "  -s, --strategies <file>   BKZ strategy table (use 'default.json' for the\n"
               "                           table installed with the library)\n"
               "\n"
               "Pruner parameters:\n"
               "      --prune-start <i>     first pruned index (default: 0)\n"
               "      --prune-end <j>       one past the last pruned index\n"
               "      --prune-pre-nodes <n> preprocessing budget in nodes (default: 1e6)\n"
               "      --prune-min-prob <p>  target expected number of solutions (default: 0.9)\n"
               "\n"
               "Output:\n"
               "  -of, --output-format <format>\n"
               "                           sequence of output items: b (basis), u (transform),\n"
               "                           v (inverse transform), c (coordinates), s (solution\n"
               "                           vector), t (status), space; append k for comma\n"
               "                           separated matrices (bk, uk, vk)\n"
               "  -h, --help                print this help\n"
               "      --version             print the version\n",
               program, fplll::LLL_DEF_DELTA, fplll::LLL_DEF_ETA, fplll::HLLL_DEF_THETA,
               fplll::HLLL_DEF_C);
}

void print_version()
{
  std::println("fplll {}\n"
               "Copyright 2005-2012 Damien Stehle, David Cade, Xavier Pujol.\n"
               "fplll is free software, licensed under the GNU Lesser General Public\n"
               "License, version 2.1 or later.",
               FPLLL_VERSION);
}

Action parse_action(std::string_view text) { return action_names.parse(text); }

LLLMethod parse_lll_method(std::string_view text) { return lll_method_names.parse(text); }

FloatType parse_float_type(std::string_view text) { return float_type_names.parse(text); }

void read_options(Arguments &arguments, Options &options)
{
  while (!arguments.empty())
  {
    const std::string_view argument = arguments.take();

    if (argument == "-h" || argument == "--help")
    {
      print_help("fplll");
      std::exit(EXIT_SUCCESS);
    }
    if (argument == "--version")
    {
      print_version();
      std::exit(EXIT_SUCCESS);
    }
    if (argument == "-a" || argument == "--action")
    {
      const std::string_view action = arguments.value(argument);
      if (action == "sdb")
      {
        options.action  = Action::bkz;
        options.variant = BKZVariant::sd_bkz;
      }
      else if (action == "sld")
      {
        options.action  = Action::bkz;
        options.variant = BKZVariant::slide;
      }
      else
      {
        options.action = parse_action(action);
      }
    }
    else if (argument == "-b" || argument == "--block-size")
    {
      options.block_size = arguments.number<int>(argument);
    }
    else if (argument == "-c" || argument == "--hlll-c")
    {
      options.hlll_c = arguments.number<double>(argument);
    }
    else if (argument == "-d" || argument == "--delta")
    {
      options.delta = arguments.number<double>(argument);
    }
    else if (argument == "-e" || argument == "--eta")
    {
      options.eta = arguments.number<double>(argument);
    }
    else if (argument == "-t" || argument == "--theta")
    {
      options.theta = arguments.number<double>(argument);
    }
    else if (argument == "-f" || argument == "--float-type")
    {
      options.float_type = parse_float_type(arguments.value(argument));
    }
    else if (argument == "-m" || argument == "--lll-method")
    {
      options.method = parse_lll_method(arguments.value(argument));
    }
    else if (argument == "-of" || argument == "--output-format")
    {
      options.output_format = arguments.value(argument);
    }
    else if (argument == "-p" || argument == "--precision")
    {
      options.precision = arguments.number<int>(argument);
    }
    else if (argument == "-s" || argument == "--strategies")
    {
      options.strategies_file = arguments.value(argument);
    }
    else if (argument == "-y" || argument == "--early-reduction")
    {
      options.early_reduction = true;
    }
    else if (argument == "--siegel")
    {
      options.siegel = true;
    }
    else if (argument == "-v" || argument == "--verbose")
    {
      options.verbose = true;
    }
    else if (argument == "--no-lll")
    {
      options.no_lll = true;
    }
    else if (argument == "--bkz-max-loops")
    {
      options.max_loops = arguments.number<int>(argument);
    }
    else if (argument == "--bkz-max-time")
    {
      const auto seconds = arguments.number<double>(argument);
      if (!std::isfinite(seconds) || seconds < 0.0)
      {
        throw std::invalid_argument("--bkz-max-time must be a non-negative finite number");
      }
      if (seconds > max_bkz_max_time_seconds)
      {
        throw std::invalid_argument(
            std::format("--bkz-max-time must be at most {:.0f} seconds (about 31.7 years)",
                        max_bkz_max_time_seconds));
      }
      options.max_time =
          std::chrono::milliseconds(static_cast<std::int64_t>(std::llround(seconds * 1000.0)));
    }
    else if (argument == "--bkz-auto-abort")
    {
      options.auto_abort = true;
    }
    else if (argument == "--bkz-bounded-lll")
    {
      options.bounded_lll = true;
    }
    else if (argument == "--bkz-gh-factor")
    {
      options.gh_factor = arguments.number<double>(argument);
    }
    else if (argument == "--prune-start")
    {
      options.prune_start = arguments.number<int>(argument);
    }
    else if (argument == "--prune-end")
    {
      options.prune_end = arguments.number<int>(argument);
    }
    else if (argument == "--prune-pre-nodes")
    {
      options.prune_pre_nodes = arguments.number<double>(argument);
    }
    else if (argument == "--prune-min-prob")
    {
      options.prune_min_probability = arguments.number<double>(argument);
    }
    else if (!argument.empty() && argument.front() == '-')
    {
      throw std::invalid_argument(std::format("invalid option '{}'; try 'fplll --help'", argument));
    }
    else
    {
      if (options.input_file)
      {
        throw std::invalid_argument("more than one input file given");
      }
      options.input_file = std::string(argument);
    }
  }
}

void validate(const Options &options)
{
  if (!std::isfinite(options.delta) || options.delta <= 0.25 || options.delta >= 1.0)
  {
    throw std::invalid_argument("delta must be a finite value in (0.25, 1)");
  }
  if (!std::isfinite(options.eta) || options.eta <= 0.5 || options.eta >= 1.0)
  {
    throw std::invalid_argument("eta must be a finite value in (0.5, 1)");
  }
  if (!std::isfinite(options.theta) || options.theta <= 0.0 || options.theta >= 1.0)
  {
    throw std::invalid_argument("theta must be a finite value in (0, 1)");
  }
  if (!std::isfinite(options.hlll_c) || options.hlll_c <= 0.0)
  {
    throw std::invalid_argument("the HLLL constant c must be a positive finite number");
  }
  if (options.precision < 0)
  {
    throw std::invalid_argument("precision must be non-negative");
  }
  if (options.max_loops && *options.max_loops < 0)
  {
    throw std::invalid_argument("--bkz-max-loops must be non-negative");
  }
  if (options.gh_factor && (!std::isfinite(*options.gh_factor) || *options.gh_factor <= 0.0))
  {
    throw std::invalid_argument("--bkz-gh-factor must be a positive finite number");
  }
  if (!std::isfinite(options.prune_pre_nodes) || options.prune_pre_nodes < 0.0)
  {
    throw std::invalid_argument("--prune-pre-nodes must be a non-negative finite number");
  }
  if (options.prune_min_probability &&
      (!std::isfinite(*options.prune_min_probability) || *options.prune_min_probability <= 0.0))
  {
    throw std::invalid_argument("--prune-min-prob must be a positive finite number");
  }
  if (!fplll::is_supported(options.float_type))
  {
    throw std::invalid_argument(
        std::format("floating-point type '{}' is not supported by this build",
                    fplll::to_string(options.float_type)));
  }
  if ((options.action == Action::bkz || options.action == Action::pruner) && options.block_size < 2)
  {
    throw std::invalid_argument("the block size must be at least 2");
  }
}

void print_matrix(const fplll::ZZ_mat<mpz_t> &matrix, bool comma)
{
  if (comma)
  {
    matrix.print_comma(std::cout);
  }
  else
  {
    std::cout << matrix << '\n';
  }
}

void print_status(RedStatus status)
{
  std::println("{}", std::to_underlying(status));
  if (!fplll::succeeded(status))
  {
    std::cerr << "fplll: failure: " << fplll::to_string(status) << '\n';
  }
}

/* Prints the requested output items.  `u`/`u_inv` and the solution vectors are
   only used by the formats that ask for them. */
void print_output(const Options &options, std::string_view default_format,
                  const fplll::ZZ_mat<mpz_t> &basis, const fplll::ZZ_mat<mpz_t> &u,
                  const fplll::ZZ_mat<mpz_t> &u_inv, RedStatus status,
                  const std::vector<fplll::Z_NR<mpz_t>> &coordinates = {},
                  const std::vector<fplll::Z_NR<mpz_t>> &solution    = {})
{
  const std::string_view format =
      options.output_format.empty() ? default_format : std::string_view(options.output_format);

  for (std::size_t i = 0; i < format.size(); i++)
  {
    const bool comma = i + 1 < format.size() && format[i + 1] == 'k';
    switch (format[i])
    {
    case 'b':
      print_matrix(basis, comma);
      break;
    case 'u':
      print_matrix(u, comma);
      break;
    case 'v':
      print_matrix(u_inv, comma);
      break;
    case 'c':
      std::cout << coordinates << '\n';
      break;
    case 's':
      std::cout << solution << '\n';
      break;
    case 't':
      print_status(status);
      break;
    case ' ':
      std::cout << '\n';
      break;
    default:
      throw std::invalid_argument(std::format("unknown output item '{}'", format[i]));
    }
    if (comma)
    {
      i++;
    }
  }
}

LLLOptions lll_options(const Options &options)
{
  return LLLOptions{
      .parameters = LLLParameters{.delta           = options.delta,
                                  .eta             = options.eta,
                                  .verbose         = options.verbose,
                                  .early_reduction = options.early_reduction,
                                  .siegel          = options.siegel},
      .method     = options.method,
      .float_type = options.float_type,
      .precision  = options.precision,
  };
}

RedStatus run_lll(const Options &options, fplll::ZZ_mat<mpz_t> &basis)
{
  const LLLOptions lll = lll_options(options);
  fplll::ZZ_mat<mpz_t> u;
  fplll::ZZ_mat<mpz_t> u_inv;

  const std::string_view format = options.output_format.empty()
                                      ? std::string_view("b")
                                      : std::string_view(options.output_format);
  const bool with_u             = format.find('u') != std::string_view::npos;
  const bool with_u_inv         = format.find('v') != std::string_view::npos;

  ReductionReport report;
  if (with_u_inv)
  {
    report = fplll::lll_reduction(basis, u, u_inv, lll);
  }
  else if (with_u)
  {
    report = fplll::lll_reduction(basis, u, lll);
  }
  else
  {
    report = fplll::lll_reduction(basis, lll);
  }

  print_output(options, "b", basis, u, u_inv, report.status);
  return report.status;
}

RedStatus run_hlll(const Options &options, fplll::ZZ_mat<mpz_t> &basis)
{
  const fplll::HLLLOptions hlll{
      .parameters = fplll::HLLLParameters{.delta   = options.delta,
                                          .eta     = options.eta,
                                          .theta   = options.theta,
                                          .c       = options.hlll_c,
                                          .verbose = options.verbose},
      .method     = options.method,
      .float_type = options.float_type,
      .precision  = options.precision,
      .no_lll     = options.no_lll,
  };
  fplll::ZZ_mat<mpz_t> u;
  fplll::ZZ_mat<mpz_t> u_inv;

  const std::string_view format = options.output_format.empty()
                                      ? std::string_view("b")
                                      : std::string_view(options.output_format);
  const bool with_u             = format.find('u') != std::string_view::npos;
  const bool with_u_inv         = format.find('v') != std::string_view::npos;

  ReductionReport report;
  if (with_u_inv)
  {
    report = fplll::hlll_reduction(basis, u, u_inv, hlll);
  }
  else if (with_u)
  {
    report = fplll::hlll_reduction(basis, u, hlll);
  }
  else
  {
    report = fplll::hlll_reduction(basis, hlll);
  }

  print_output(options, "b", basis, u, u_inv, report.status);
  return report.status;
}

BKZParam bkz_parameter(const Options &options, const std::vector<fplll::Strategy> &strategies)
{
  return BKZParam{
      .block_size  = options.block_size,
      .strategies  = strategies,
      .delta       = options.delta,
      .variant     = options.variant,
      .verbose     = options.verbose,
      .no_lll      = options.no_lll,
      .bounded_lll = options.bounded_lll,
      .auto_abort  = options.auto_abort,
      .gh_factor   = options.gh_factor,
      .max_loops   = options.max_loops,
      .max_time    = options.max_time,
      .float_type  = options.float_type,
      .precision   = options.precision,
  };
}

RedStatus run_bkz(const Options &options, fplll::ZZ_mat<mpz_t> &basis)
{
  std::vector<fplll::Strategy> strategies;
  if (!options.strategies_file.empty())
  {
    strategies = fplll::load_strategies_json(options.strategies_file);
  }

  const BKZParam param = bkz_parameter(options, strategies);
  fplll::ZZ_mat<mpz_t> u;

  const std::string_view format = options.output_format.empty()
                                      ? std::string_view("b")
                                      : std::string_view(options.output_format);
  const ReductionReport report  = format.find('u') != std::string_view::npos
                                      ? fplll::bkz_reduction(basis, u, param)
                                      : fplll::bkz_reduction(basis, param);

  print_output(options, "b", basis, u, fplll::ZZ_mat<mpz_t>(), report.status);
  return report.status;
}

RedStatus run_hkz(const Options &options, fplll::ZZ_mat<mpz_t> &basis)
{
  const HKZOptions hkz{
      .float_type = options.float_type,
      .precision  = options.precision,
      .verbose    = options.verbose,
  };
  const ReductionReport report = fplll::hkz_reduction(basis, hkz);
  print_output(options, "b", basis, fplll::ZZ_mat<mpz_t>(), fplll::ZZ_mat<mpz_t>(), report.status);
  return report.status;
}

RedStatus run_svpcvp(const Options &options, fplll::ZZ_mat<mpz_t> &basis,
                     const std::vector<fplll::Z_NR<mpz_t>> &target)
{
  std::vector<fplll::Z_NR<mpz_t>> coordinates;
  std::vector<fplll::Z_NR<mpz_t>> solution;
  fplll::ZZ_mat<mpz_t> u;

  const std::string_view format = options.output_format.empty()
                                      ? std::string_view("s")
                                      : std::string_view(options.output_format);
  const bool with_coordinates   = format.find('c') != std::string_view::npos;
  const bool with_solution      = format.find('s') != std::string_view::npos;

  if (!options.no_lll)
  {
    const LLLOptions lll = lll_options(options);
    const ReductionReport lll_report =
        with_coordinates ? fplll::lll_reduction(basis, u, lll) : fplll::lll_reduction(basis, lll);
    if (!fplll::succeeded(lll_report))
    {
      std::cerr << "fplll: LLL reduction failed: " << fplll::to_string(lll_report.status) << '\n';
      return lll_report.status;
    }
  }

  RedStatus status;
  std::vector<fplll::Z_NR<mpz_t>> coordinates_in_reduced;
  if (target.empty())
  {
    status =
        fplll::shortest_vector(basis, coordinates_in_reduced,
                               SVPOptions{.method = SVPMethod::proved, .verbose = options.verbose});
  }
  else
  {
    status = fplll::closest_vector(
        basis, target, coordinates_in_reduced,
        CVPOptions{.method = fplll::CVPMethod::proved, .verbose = options.verbose});
  }
  if (!fplll::succeeded(status))
  {
    std::cerr << "fplll: failure: " << fplll::to_string(status) << '\n';
    return status;
  }

  if (with_coordinates)
  {
    if (options.no_lll)
    {
      coordinates = coordinates_in_reduced;
    }
    else
    {
      fplll::vector_matrix_product(coordinates, coordinates_in_reduced, u);
    }
  }
  if (with_solution)
  {
    fplll::vector_matrix_product(solution, coordinates_in_reduced, basis);
  }

  print_output(options, "s", basis, fplll::ZZ_mat<mpz_t>(), fplll::ZZ_mat<mpz_t>(), status,
               coordinates, solution);
  return status;
}

RedStatus run_pruner(const Options &options, fplll::ZZ_mat<mpz_t> &basis)
{
  const fplll::PrunerOptions pruner{
      .float_type      = options.float_type,
      .precision       = options.precision,
      .start           = options.prune_start,
      .end             = options.prune_end,
      .pre_nodes       = options.prune_pre_nodes,
      .min_probability = options.prune_min_probability,
      .gh_factor       = options.gh_factor.value_or(1.0),
      .verbose         = options.verbose,
  };
  const RedStatus status = fplll::run_pruner(basis, pruner);
  print_output(options, "t", basis, fplll::ZZ_mat<mpz_t>(), fplll::ZZ_mat<mpz_t>(), status);
  return status;
}

RedStatus run(const Options &options, fplll::ZZ_mat<mpz_t> &basis,
              const std::vector<fplll::Z_NR<mpz_t>> &target)
{
  switch (options.action)
  {
  case Action::lll:
    return run_lll(options, basis);
  case Action::hlll:
    return run_hlll(options, basis);
  case Action::bkz:
    return run_bkz(options, basis);
  case Action::hkz:
    return run_hkz(options, basis);
  case Action::svp:
  case Action::cvp:
    return run_svpcvp(options, basis, target);
  case Action::pruner:
    return run_pruner(options, basis);
  }
  throw std::invalid_argument("unhandled action");
}

}  // namespace

int main(int argc, char **argv)
{
  try
  {
    Options options;
    Arguments arguments(argc, argv);
    read_options(arguments, options);
    validate(options);

    std::optional<std::ifstream> file;
    std::istream *input = &std::cin;
    if (options.input_file)
    {
      file.emplace(*options.input_file);
      if (!file->is_open())
      {
        throw std::runtime_error(std::format("cannot open input file '{}'", *options.input_file));
      }
      input = &*file;
    }

    fplll::ZZ_mat<mpz_t> basis;
    *input >> basis;
    std::vector<fplll::Z_NR<mpz_t>> target;
    if (options.action == Action::cvp)
    {
      *input >> target;
      // An explicit but empty target is malformed; do not silently fall back to
      // the SVP entry point.
      if (*input && target.empty())
      {
        throw std::runtime_error("invalid input: CVP requires a non-empty target vector");
      }
    }
    if (!*input)
    {
      throw std::runtime_error("invalid input matrix");
    }

    fplll::set_log_stream(options.verbose || options.action == Action::pruner ? &std::cerr
                                                                              : nullptr);
    const RedStatus status = run(options, basis, target);
    return fplll::succeeded(status) ? EXIT_SUCCESS : EXIT_FAILURE;
  }
  catch (const std::exception &error)
  {
    // Library errors already start with "fplll: "; do not repeat the prefix.
    const std::string_view message = error.what();
    if (!message.starts_with("fplll: "))
    {
      std::cerr << "fplll: ";
    }
    std::cerr << message << '\n';
    return EXIT_FAILURE;
  }
}
