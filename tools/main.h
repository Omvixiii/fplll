/* Copyright (C) 2005-2008 Damien Stehle.
   Copyright (C) 2007 David Cade.
   Copyright (C) 2008-2011 Xavier Pujol.

   This file is part of fplll. fplll is free software: you
   can redistribute it and/or modify it under the terms of the GNU Lesser
   General Public License as published by the Free Software Foundation,
   either version 2.1 of the License, or (at your option) any later version.

   fplll is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with fplll. If not, see <http://www.gnu.org/licenses/>. */

#ifndef FPLLL_MAIN_H
#define FPLLL_MAIN_H

#include <charconv>
#include <cstring>
#include <fplll/fplll.h>
#include <system_error>

#define ABORT_MSG(y)                                                                               \
  {                                                                                                \
    std::cerr << "fplll: " << y << '\n';                                                           \
    exit(1);                                                                                       \
  }
#define CHECK(x, y)                                                                                \
  if (!(x))                                                                                        \
  ABORT_MSG(y)

/** Parses a complete numeric argument, rejecting trailing characters. */
template <class T> T parse_option(const char *text, const char *option)
{
  T value{};
  const char *end                     = text + std::strlen(text);
  const std::from_chars_result result = std::from_chars(text, end, value);
  if (result.ec != std::errc() || result.ptr != end)
    ABORT_MSG("invalid value for " << option << ": '" << text << "'");
  return value;
}

using namespace std;
using namespace fplll;

enum Action
{
  ACTION_LLL,
  ACTION_HKZ,
  ACTION_BKZ,
  ACTION_SVP,
  ACTION_CVP,
  ACTION_HLLL,
  ACTION_PRU
};

struct Options
{
  Options()
      : action(ACTION_LLL), method(LM_WRAPPER), int_type(ZT_MPZ), float_type(FT_DEFAULT),
        delta(LLL_DEF_DELTA), eta(LLL_DEF_ETA), precision(0), early_red(false), siegel(false),
        no_lll(false), block_size(0), bkz_flags(0), bkz_max_loops(0), prune_start(0), prune_end(0),
        prune_pre_nodes(0.0), prune_min_prob(0.0), bkz_max_time(0.0), bkz_gh_factor(1.1),
        verbose(false), input_file(nullptr), output_format(nullptr), theta(HLLL_DEF_THETA),
        c(HLLL_DEF_C)
  {
  }
  Action action;
  LLLMethod method;
  IntType int_type;
  FloatType float_type;
  double delta;
  double eta;
  int precision;
  bool early_red;
  bool siegel;

  bool no_lll;
  int block_size;
  int bkz_flags;
  int bkz_max_loops;
  int prune_start;
  int prune_end;
  double prune_pre_nodes;
  double prune_min_prob;
  double bkz_max_time;
  string bkz_dump_gso_filename;
  double bkz_gh_factor;
  string bkz_strategy_file;

  bool verbose;
  const char *input_file;
  const char *output_format;

  double theta;
  double c;
};

#endif
