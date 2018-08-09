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

#include <caf/ip_address.hpp>

#include <caf/detail/parser/read_ipv6_address.hpp>

#include "vast/access.hpp"
#include "vast/concept/parseable/core.hpp"
#include "vast/concept/parseable/numeric/integral.hpp"
#include "vast/concept/parseable/string/char_class.hpp"

namespace vast {

template <>
struct access::parser<caf::ip_address>
  : vast::parser<access::parser<caf::ip_address>> {
  using attribute = caf::ip_address;

  struct consumer {
    consumer(attribute& dest) : ref(dest) {
      // nop
    }

    void value(attribute x) {
      ref = x;
    }

    attribute& ref;
  };

  template <class Iterator>
  bool parse(Iterator& f, const Iterator& l, attribute& a) const {
    caf::detail::parser::state<Iterator> res{f, l};
    consumer g{a};
    caf::detail::parser::read_ipv6_address(res, g);
    if (res.code <= caf::pec::trailing_character) {
      f = res.i;
      return true;
    }
    return false;
  }

  template <class Iterator>
  bool parse(Iterator& f, const Iterator& l, unused_type) const {
    attribute tmp;
    return parse(f, l, tmp);
  }
};

template <>
struct parser_registry<caf::ip_address> {
  using type = access::parser<caf::ip_address>;
};

namespace parsers {

static auto const addr = make_parser<caf::ip_address>();

} // namespace parsers

} // namespace vast

