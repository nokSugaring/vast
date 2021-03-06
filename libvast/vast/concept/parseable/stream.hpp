/******************************************************************************
 *                    _   _____   __________                                  *
 *                   | | / / _ | / __/_  __/     Visibility                   *
 *                   | |/ / __ |_\ \  / /          Across                     *
 *                   |___/_/ |_/___/ /_/       Space and Time                 *
 *                                                                            *
 * This file is part of VAST. It is subject to the license terms in the       *
 * LICENSE file found in the top-level directory of this distribution and at  *
 * http://vast.io/license. No part of VAST, including this file, may be       *
 * copied, modified, propagated, or distributed except according to the terms *
 * contained in the LICENSE file.                                             *
 ******************************************************************************/

#pragma once

#include <istream>
#include <type_traits>

#include "vast/concept/parseable/parse.hpp"

namespace vast {

template <class CharT, class Traits, class T>
auto operator>>(std::basic_istream<CharT, Traits>& in, T& x)
  -> std::enable_if_t<is_parseable_v<std::istreambuf_iterator<CharT>, T>,
                      decltype(in)> {
  using vast::parse; // enable ADL
  std::istreambuf_iterator<CharT> begin{in}, end;
  if (!parse(begin, end, x))
    in.setstate(std::ios_base::failbit);
  return in;
}

} // namespace vast

