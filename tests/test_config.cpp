/* Copyright (C) 2026 The fplll development team

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

#include <fplll/fplll.h>

#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace fplll;

namespace
{

std::string write_strategy_file(const char *name, const std::string &content)
{
  const std::string path = std::string("test_config_") + name + ".json";
  std::ofstream out(path);
  out << content;
  return path;
}

template <class F> bool throws(F function)
{
  try
  {
    function();
  }
  catch (...)
  {
    return true;
  }
  return false;
}

}  // namespace

int main()
{
  bool ok = true;

  const std::string valid =
      R"([{"block_size": 1, "preprocessing_block_sizes": [],)"
      R"( "pruning_parameters": [[1.0, [1.0], 1.0]]}])";
  const std::string valid_path = write_strategy_file("valid", valid);
  {
    std::vector<Strategy> strategies = load_strategies_json(valid_path);
    if (strategies.size() != 2)
      ok = false;

    // A strategy table that does not cover the requested block size must be
    // rejected instead of indexing out of bounds.
    ok = ok && throws([&] { const BKZParam param(3, strategies); (void)param; });
  }

  const std::string missing =
      R"([{"pruning_parameters": [[1.0, [1.0], 1.0]]}])";
  const std::string missing_path = write_strategy_file("missing", missing);
  ok = ok && throws([&] { (void)load_strategies_json(missing_path); });

  const std::string not_array = R"({"block_size": 1})";
  const std::string not_array_path = write_strategy_file("not_array", not_array);
  ok = ok && throws([&] { (void)load_strategies_json(not_array_path); });

  const std::string bad_json = "{not json";
  const std::string bad_json_path = write_strategy_file("bad_json", bad_json);
  ok = ok && throws([&] { (void)load_strategies_json(bad_json_path); });

  std::remove(valid_path.c_str());
  std::remove(missing_path.c_str());
  std::remove(not_array_path.c_str());
  std::remove(bad_json_path.c_str());

  if (!ok)
  {
    std::fprintf(stderr, "test_config failed\n");
    return 1;
  }
  return 0;
}
