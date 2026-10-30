// SPDX-FileCopyrightText: 2005-2008 Damien Stehle
// SPDX-FileCopyrightText: 2007 David Cade
// SPDX-FileCopyrightText: 2008-2011 Xavier Pujol
// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef FPLLL_TOOL_CLI_H
#define FPLLL_TOOL_CLI_H

#include <charconv>
#include <cstddef>
#include <format>
#include <stdexcept>
#include <string_view>
#include <system_error>

namespace cli
{

/** Parses text as T, rejecting trailing characters. */
template <class T> T parse_value(std::string_view text, std::string_view name)
{
  T value{};
  const std::from_chars_result result =
      std::from_chars(text.data(), text.data() + text.size(), value);
  if (result.ec != std::errc() || result.ptr != text.data() + text.size())
  {
    throw std::invalid_argument(std::format("invalid value for '{}': '{}'", name, text));
  }
  return value;
}

}  // namespace cli

/** Argument cursor shared by the command-line tools.  Every accessor either
    returns a value or throws std::invalid_argument with a message that names
    the offending option; the tool entry points turn that into a clean exit. */
class Arguments
{
public:
  Arguments(int argc, char **argv) : argc_(argc), argv_(argv) {}

  [[nodiscard]] bool empty() const { return index_ >= argc_; }

  /** Returns the next argument without consuming it. */
  [[nodiscard]] std::string_view peek() const
  {
    if (empty())
    {
      throw std::invalid_argument("missing argument");
    }
    return {argv_[index_]};
  }

  std::string_view take()
  {
    if (empty())
    {
      throw std::invalid_argument("missing argument");
    }
    return {argv_[index_++]};
  }

  /** Returns the value of an option that requires one. */
  std::string_view value(std::string_view option)
  {
    if (empty())
    {
      throw std::invalid_argument(std::format("option '{}' requires a value", option));
    }
    return take();
  }

  /** Parses the value of an option as T, rejecting trailing characters. */
  template <class T> T number(std::string_view option)
  {
    return cli::parse_value<T>(value(option), option);
  }

private:
  int argc_;
  char **argv_;
  int index_ = 1;
};

#endif
