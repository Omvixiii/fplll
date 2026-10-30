// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#if !defined(FPLLL_TOOL) || !defined(LATTICEGEN_TOOL)
#error "FPLLL_TOOL and LATTICEGEN_TOOL must be defined by the build system"
#endif

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#if !defined(_WIN32)
#include <sys/wait.h>
#endif

namespace
{

constexpr std::string_view matrix_6x7 = "[[1 0 0 0 0 0 0][0 1 0 0 0 0 0][0 0 1 0 0 0 0]"
                                        "[0 0 0 1 0 0 0][0 0 0 0 1 0 0][0 0 0 0 0 1 0]]\n";

constexpr const char *output_file = "test_cli_output.txt";

struct tools
{
  std::string fplll      = FPLLL_TOOL;
  std::string latticegen = LATTICEGEN_TOOL;
};

struct run_result
{
  int exit_code;
  int signal;
};

std::string shell_quote(std::string_view text)
{
#if defined(_WIN32)
  return '"' + std::string(text) + '"';
#else
  return '\'' + std::string(text) + '\'';
#endif
}

std::string write_file(std::string_view path, std::string_view content)
{
  std::ofstream out{std::string(path)};
  out << content;
  return std::string(path);
}

std::string command_line(std::string_view program, const std::vector<std::string> &arguments,
                         std::string_view input)
{
  std::string line = shell_quote(program);
  for (const auto &argument : arguments)
  {
    line += ' ' + shell_quote(argument);
  }
  return line + " < " + shell_quote(input) + " > " + shell_quote(output_file) + " 2>&1";
}

run_result run_command(const std::string &line)
{
  // This test harness deliberately runs the built fplll command line tools; the
  // arguments are shell-quoted above.
  const int status = std::system(line.c_str());  // NOLINT(bugprone-command-processor)
#if defined(_WIN32)
  return {status, 0};
#else
  if (status == -1)
  {
    return {.exit_code = -1, .signal = 0};
  }
  if (WIFSIGNALED(status))
  {
    return {.exit_code = -1, .signal = WTERMSIG(status)};
  }
  return {.exit_code = WEXITSTATUS(status), .signal = 0};
#endif
}

void check(std::string_view name, std::string_view program,
           const std::vector<std::string> &arguments, std::string_view input, bool expect_success)
{
  INFO("case: " << name);
  const run_result result = run_command(command_line(program, arguments, input));
  if (result.signal != 0)
  {
    FAIL("killed by signal " << result.signal);
  }
  if (result.exit_code < 0)
  {
    FAIL("could not run");
  }
  if (expect_success)
  {
    CHECK(result.exit_code == 0);
  }
  else
  {
    CHECK(result.exit_code != 0);
  }
}

/** Runs a command that must fail and checks that the combined output mentions
    `expected_message`.  The fixture redirects both stdout and stderr into
    `output_file`, so this also verifies the message the user sees. */
void check_failure_message(std::string_view name, std::string_view program,
                           const std::vector<std::string> &arguments, std::string_view input,
                           std::string_view expected_message)
{
  INFO("case: " << name);
  const run_result result = run_command(command_line(program, arguments, input));
  if (result.signal != 0)
  {
    FAIL("killed by signal " << result.signal);
  }
  if (result.exit_code < 0)
  {
    FAIL("could not run");
  }
  CHECK(result.exit_code != 0);
  std::ifstream output{std::string(output_file)};
  const std::string captured{std::istreambuf_iterator<char>(output),
                             std::istreambuf_iterator<char>()};
  INFO("captured output: " << captured);
  CHECK(captured.find(expected_message) != std::string::npos);
}

std::string strategy_file(std::string_view name, std::string_view content)
{
  return write_file("test_cli_" + std::string(name) + ".json", content);
}

// Writes the CLI inputs into the test working directory and removes them again.
struct cli_fixture
{
  cli_fixture()
      : empty_input(write_file("test_cli_empty.txt", "")),
        matrix_input(write_file("test_cli_matrix.txt", matrix_6x7)),
        one_row_input(write_file("test_cli_one_row.txt", "[[1 2]]\n")),
        zero_input(write_file("test_cli_zero.txt", "[[0 0][0 0]]\n")),
        empty_matrix_input(write_file("test_cli_empty_matrix.txt", "[]\n")),
        zero_columns_input(write_file("test_cli_zero_columns.txt", "[[]]\n")),
        cvp_input(write_file("test_cli_cvp.txt", "[[10 11][11 12]]\n[1 0]\n")),
        empty_target_input(write_file("test_cli_empty_target.txt", "[[10 11][11 12]][]\n")),
        short_strategy(strategy_file(
            "short", R"([{"block_size": 1, "pruning_parameters": [[1.0, [1.0], 1.0]]}])")),
        missing_block_size(
            strategy_file("missing", R"([{"pruning_parameters": [[1.0, [1.0], 1.0]]}])")),
        not_array(strategy_file("not_array", R"({"block_size": 1})")),
        bad_json(strategy_file("bad", "{not json"))
  {
  }

  ~cli_fixture()
  {
    // No allocation here: a destructor must not throw.
    std::remove(empty_input.c_str());
    std::remove(matrix_input.c_str());
    std::remove(one_row_input.c_str());
    std::remove(zero_input.c_str());
    std::remove(empty_matrix_input.c_str());
    std::remove(zero_columns_input.c_str());
    std::remove(cvp_input.c_str());
    std::remove(empty_target_input.c_str());
    std::remove(short_strategy.c_str());
    std::remove(missing_block_size.c_str());
    std::remove(not_array.c_str());
    std::remove(bad_json.c_str());
    std::remove(output_file);
  }

  cli_fixture(const cli_fixture &)            = delete;
  cli_fixture &operator=(const cli_fixture &) = delete;

  tools executables;
  std::string empty_input;
  std::string matrix_input;
  std::string one_row_input;
  std::string zero_input;
  std::string empty_matrix_input;
  std::string zero_columns_input;
  std::string cvp_input;
  std::string empty_target_input;
  std::string short_strategy;
  std::string missing_block_size;
  std::string not_array;
  std::string bad_json;
};

}  // namespace

TEST_CASE_METHOD(cli_fixture, "latticegen accepts valid parameters", "[cli]")
{
  check("latticegen --help", executables.latticegen, {"--help"}, empty_input, true);
  check("latticegen --version", executables.latticegen, {"--version"}, empty_input, true);
  check("latticegen r valid", executables.latticegen, {"r", "10", "20"}, empty_input, true);
  check("latticegen q valid", executables.latticegen, {"q", "5", "3", "10", "b"}, empty_input,
        true);
  check("latticegen seeded", executables.latticegen, {"-randseed", "42", "u", "5", "10"},
        empty_input, true);
}

TEST_CASE_METHOD(cli_fixture, "latticegen rejects invalid generator parameters", "[cli]")
{
  // Invalid generator parameters used to crash or produce meaningless matrices.
  check("latticegen negative bits r", executables.latticegen, {"r", "10", "-5"}, empty_input,
        false);
  check("latticegen negative bits u", executables.latticegen, {"u", "10", "-5"}, empty_input,
        false);
  check("latticegen negative bits s", executables.latticegen, {"s", "10", "-5", "3"}, empty_input,
        false);
  check("latticegen negative alpha t", executables.latticegen, {"t", "10", "-5"}, empty_input,
        false);
  check("latticegen zero dimension", executables.latticegen, {"r", "0", "10"}, empty_input, false);
  check("latticegen huge dimension", executables.latticegen, {"r", "2147483647", "5"}, empty_input,
        false);
  check("latticegen qary k out of range", executables.latticegen, {"q", "5", "10", "b", "3"},
        empty_input, false);
  check("latticegen unknown method", executables.latticegen, {"z", "5", "10"}, empty_input, false);
  check("latticegen unknown option", executables.latticegen, {"--bogus", "r", "5", "10"},
        empty_input, false);
}

TEST_CASE_METHOD(cli_fixture, "fplll reduces valid inputs", "[cli]")
{
  check("fplll --help", executables.fplll, {"--help"}, empty_input, true);
  check("fplll --version", executables.fplll, {"--version"}, empty_input, true);
  check("fplll lll", executables.fplll, {"-a", "lll"}, matrix_input, true);
  check("fplll lll with transform", executables.fplll, {"-a", "lll", "-of", "bku"}, matrix_input,
        true);
  check("fplll hlll one row", executables.fplll, {"-a", "hlll"}, one_row_input, true);
  check("fplll bkz zero matrix", executables.fplll, {"-a", "bkz", "-b", "2"}, zero_input, true);
  check("fplll cvp", executables.fplll, {"-a", "cvp"}, cvp_input, true);
  // The comma output format handles a basis with no rows or no columns.
  check("fplll empty basis comma output", executables.fplll, {"-a", "lll", "-of", "bk"},
        empty_matrix_input, true);
  check("fplll zero-column basis comma output", executables.fplll, {"-a", "lll", "-of", "bk"},
        zero_columns_input, true);
  check("fplll bkz sd variant", executables.fplll, {"-a", "sdb", "-b", "2"}, matrix_input, true);
  check("fplll bkz max time fractional", executables.fplll,
        {"-a", "bkz", "-b", "2", "--bkz-max-time", "0.5"}, matrix_input, true);
  check("fplll bkz max time at limit", executables.fplll,
        {"-a", "bkz", "-b", "2", "--bkz-max-time", "1e9"}, matrix_input, true);
}

TEST_CASE_METHOD(cli_fixture, "fplll rejects invalid options", "[cli]")
{
  // Invalid scalar options.
  check("fplll delta out of range", executables.fplll, {"-d", "2"}, matrix_input, false);
  check("fplll delta nan", executables.fplll, {"-d", "nan"}, matrix_input, false);
  check("fplll eta out of range", executables.fplll, {"-e", "2"}, matrix_input, false);
  check("fplll negative precision", executables.fplll, {"-p", "-5"}, matrix_input, false);
  check("fplll negative hlll constant", executables.fplll, {"-c", "-1"}, matrix_input, false);
  check("fplll block size too small", executables.fplll, {"-a", "bkz", "-b", "1"}, matrix_input,
        false);
  check("fplll unsupported float type", executables.fplll, {"-f", "monster"}, matrix_input, false);
  check("fplll unknown option", executables.fplll, {"--bogus"}, matrix_input, false);
  check("fplll missing input file", executables.fplll, {"/no/such/input/file"}, empty_input, false);
  check("fplll cvp empty target", executables.fplll, {"-a", "cvp"}, empty_target_input, false);
  check_failure_message("fplll cvp empty target message", executables.fplll, {"-a", "cvp"},
                        empty_target_input, "CVP requires a non-empty target vector");

  // E-11: --bkz-max-time is bounded to keep the conversion to milliseconds
  // exact instead of silently saturating on huge values; the limit is 1e9
  // seconds (~31.7 years), the error names the limit.
  check("fplll bkz max time negative", executables.fplll,
        {"-a", "bkz", "-b", "2", "--bkz-max-time", "-1"}, matrix_input, false);
  check("fplll bkz max time nan", executables.fplll,
        {"-a", "bkz", "-b", "2", "--bkz-max-time", "nan"}, matrix_input, false);
  check("fplll bkz max time inf", executables.fplll,
        {"-a", "bkz", "-b", "2", "--bkz-max-time", "inf"}, matrix_input, false);
  check("fplll bkz max time above limit", executables.fplll,
        {"-a", "bkz", "-b", "2", "--bkz-max-time", "1e10"}, matrix_input, false);
  check("fplll bkz max time 1e19", executables.fplll,
        {"-a", "bkz", "-b", "2", "--bkz-max-time", "1e19"}, matrix_input, false);
  check("fplll bkz max time 1e300", executables.fplll,
        {"-a", "bkz", "-b", "2", "--bkz-max-time", "1e300"}, matrix_input, false);
  check_failure_message("fplll bkz max time above limit message", executables.fplll,
                        {"-a", "bkz", "-b", "2", "--bkz-max-time", "1e19"}, matrix_input,
                        "at most 1000000000 seconds");

  // Every floating-point tool option validates that its value is finite before
  // the reduction starts.
  check("fplll prune nodes inf", executables.fplll,
        {"-a", "pru", "-b", "2", "--prune-pre-nodes", "inf"}, matrix_input, false);
  check("fplll prune nodes nan", executables.fplll,
        {"-a", "pru", "-b", "2", "--prune-pre-nodes", "nan"}, matrix_input, false);
  check("fplll prune nodes negative", executables.fplll,
        {"-a", "pru", "-b", "2", "--prune-pre-nodes", "-1"}, matrix_input, false);
  check("fplll prune min prob nan", executables.fplll,
        {"-a", "pru", "-b", "2", "--prune-min-prob", "nan"}, matrix_input, false);
  check("fplll prune min prob inf", executables.fplll,
        {"-a", "pru", "-b", "2", "--prune-min-prob", "inf"}, matrix_input, false);
  check("fplll prune min prob non-positive", executables.fplll,
        {"-a", "pru", "-b", "2", "--prune-min-prob", "-0.5"}, matrix_input, false);
}

TEST_CASE_METHOD(cli_fixture, "fplll accepts a zero BKZ time limit and stops at the first tour",
                 "[cli]")
{
  // `--bkz-max-time 0` is a valid limit, not a parsing error: BKZ stops at the
  // first tour boundary and reports `bkz_time_limit`, which makes the CLI exit
  // with the documented failure status.
  check_failure_message("fplll bkz max time zero", executables.fplll,
                        {"-a", "bkz", "-b", "2", "--bkz-max-time", "0", "-of", "t"}, matrix_input,
                        "time limit exceeded in BKZ");
}

TEST_CASE_METHOD(cli_fixture, "fplll rejects invalid strategy tables", "[cli]")
{
  check("fplll short strategy table", executables.fplll,
        {"-a", "bkz", "-b", "5", "--strategies", short_strategy}, matrix_input, false);
  check("fplll missing block_size", executables.fplll,
        {"-a", "bkz", "-b", "2", "--strategies", missing_block_size}, matrix_input, false);
  check("fplll strategy root not array", executables.fplll,
        {"-a", "bkz", "-b", "2", "--strategies", not_array}, matrix_input, false);
  check("fplll malformed strategy json", executables.fplll,
        {"-a", "bkz", "-b", "2", "--strategies", bad_json}, matrix_input, false);
}
