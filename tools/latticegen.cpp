// SPDX-FileCopyrightText: 2005-2008 Damien Stehle
// SPDX-FileCopyrightText: 2007 David Cade
// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "cli.h"

#include <fplll/fplll_config.h>
#include <fplll/nr/matrix.h>
#include <fplll/nr/nr.h>

#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <format>
#include <iostream>
#include <mpfr.h>
#include <print>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{

using fplll::FP_NR;
using fplll::RandGen;
using fplll::Z_NR;
using fplll::ZZ_mat;

void print_help()
{
  std::println("Usage: latticegen [-randseed <seed|time>] <method> <dimension> [arguments]\n"
               "\n"
               "Generates a test lattice and writes it to stdout, one basis vector per row.\n"
               "\n"
               "Methods:\n"
               "  r <d> <bits>               integer relations matrix (d x d+1)\n"
               "  s <d> <bits> <bits2>       simultaneous Diophantine matrix (d+1 x d+1)\n"
               "  u <d> <bits>               uniform matrix (d x d)\n"
               "  n <d> <value> <b|q>        NTRU-like matrix (2d x 2d)\n"
               "  N <d> <value> <b|q>        NTRU-like matrix, second variant (2d x 2d)\n"
               "  q <d> <k> <value> <b|q|p>  q-ary matrix (d x d)\n"
               "  t <d> <alpha>              truncated Gaussian matrix (d x d)\n"
               "  T <d>                      truncated Gaussian, weights read from stdin\n"
               "\n"
               "For the b/q/p choice, b means the value is a bit size, q a modulus, and p a\n"
               "prime modulus.  -randseed <int> fixes the random stream, -randseed time seeds\n"
               "it from the clock.\n");
}

void print_version()
{
  std::println("latticegen (fplll) {}\n"
               "Copyright 2005-2011 Damien Stehle, David Cade, Xavier Pujol.\n"
               "fplll is free software, licensed under the GNU Lesser General Public\n"
               "License, version 2.1 or later.",
               FPLLL_VERSION);
}

[[noreturn]] void invalid_argument(std::string_view message)
{
  throw std::invalid_argument(std::string(message));
}

int positive_bits(std::string_view text, std::string_view name)
{
  const int bits = cli::parse_value<int>(text, name);
  if (bits <= 0)
  {
    invalid_argument(std::format("{} must be at least 1", name));
  }
  return bits;
}

Z_NR<mpz_t> positive_integer(std::string_view text, std::string_view name)
{
  Z_NR<mpz_t> value;
  const std::string text_string(text);
  if (!value.set_str(text_string.c_str()) || value <= 0)
  {
    invalid_argument(std::format("{} must be a positive integer", name));
  }
  return value;
}

char mode_letter(std::string_view text, std::string_view name)
{
  if (text.empty())
  {
    invalid_argument(std::format("{} must not be empty", name));
  }
  return text.front();
}

int generate(std::string_view method, int dimension, Arguments &arguments)
{
  const int d = dimension;
  ZZ_mat<mpz_t> matrix;

  if (method == "r")
  {
    const int bits = positive_bits(arguments.value("bits"), "bits");
    matrix.resize(d, d + 1);
    matrix.gen_intrel(bits);
  }
  else if (method == "s")
  {
    const int bits  = positive_bits(arguments.value("bits"), "bits");
    const int bits2 = positive_bits(arguments.value("bits2"), "bits2");
    matrix.resize(d + 1, d + 1);
    matrix.gen_simdioph(bits, bits2);
  }
  else if (method == "u")
  {
    const int bits = positive_bits(arguments.value("bits"), "bits");
    matrix.resize(d, d);
    matrix.gen_uniform(bits);
  }
  else if (method == "n" || method == "N")
  {
    const std::string_view value = arguments.value("value");
    const char mode              = mode_letter(arguments.value("b|q"), "mode");
    matrix.resize(2 * d, 2 * d);
    if (mode == 'b')
    {
      const int bits = positive_bits(value, "value");
      if (method == "n")
      {
        matrix.gen_ntrulike_bits(bits);
      }
      else
      {
        matrix.gen_ntrulike2_bits(bits);
      }
    }
    else if (mode == 'q')
    {
      const Z_NR<mpz_t> modulus = positive_integer(value, "value");
      if (method == "n")
      {
        matrix.gen_ntrulike(modulus);
      }
      else
      {
        matrix.gen_ntrulike2(modulus);
      }
    }
    else
    {
      invalid_argument("mode must be 'b' or 'q'");
    }
  }
  else if (method == "q")
  {
    const int k                  = cli::parse_value<int>(arguments.value("k"), "k");
    const std::string_view value = arguments.value("value");
    const char mode              = mode_letter(arguments.value("b|q|p"), "mode");
    if (k < 0 || k > d)
    {
      invalid_argument("k must be between 0 and the dimension");
    }
    matrix.resize(d, d);
    if (mode == 'b')
    {
      matrix.gen_qary_bits(k, positive_bits(value, "value"));
    }
    else if (mode == 'q')
    {
      matrix.gen_qary(k, positive_integer(value, "value"));
    }
    else if (mode == 'p')
    {
      matrix.gen_qary_prime(k, positive_bits(value, "value"));
    }
    else
    {
      invalid_argument("mode must be 'b', 'q' or 'p'");
    }
  }
  else if (method == "t")
  {
    const auto alpha = cli::parse_value<double>(arguments.value("alpha"), "alpha");
    if (!std::isfinite(alpha) || alpha <= 0.0)
    {
      invalid_argument("alpha must be a positive finite number");
    }
    matrix.resize(d, d);
    matrix.gen_trg(alpha);
  }
  else if (method == "T")
  {
    std::vector<FP_NR<mpfr_t>> weights(static_cast<std::size_t>(d));
    for (auto &weight : weights)
    {
      if (mpfr_inp_str(weight.get_data(), stdin, 10, GMP_RNDN) == 0)
      {
        invalid_argument("could not read the truncated Gaussian weights from stdin");
      }
    }
    matrix.resize(d, d);
    matrix.gen_trg2(weights.data());
  }
  else
  {
    invalid_argument(std::format("unknown generation method '{}'", method));
  }

  std::cout << matrix << '\n';
  return EXIT_SUCCESS;
}

int run(int argc, char **argv)
{
  Arguments arguments(argc, argv);

  while (!arguments.empty() && arguments.peek().starts_with('-'))
  {
    const std::string_view option = arguments.take();
    if (option == "-h" || option == "--help")
    {
      print_help();
      return EXIT_SUCCESS;
    }
    if (option == "--version")
    {
      print_version();
      return EXIT_SUCCESS;
    }
    if (option == "-randseed")
    {
      const std::string_view seed = arguments.value(option);
      if (seed == "time")
      {
        RandGen::init_with_time();
      }
      else
      {
        RandGen::init_with_seed(cli::parse_value<unsigned long>(seed, option));
      }
      continue;
    }
    invalid_argument(std::format("invalid option '{}'; try 'latticegen --help'", option));
  }

  if (arguments.empty())
  {
    invalid_argument("a generation method and a dimension are required");
  }
  const std::string_view method = arguments.take();
  const int dimension           = arguments.number<int>("dimension");
  if (dimension < 1 || dimension > INT_MAX / 2)
  {
    invalid_argument("dimension must be a positive integer of reasonable size");
  }

  return generate(method, dimension, arguments);
}

}  // namespace

int main(int argc, char **argv)
{
  try
  {
    return run(argc, argv);
  }
  catch (const std::exception &error)
  {
    std::cerr << "latticegen: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
