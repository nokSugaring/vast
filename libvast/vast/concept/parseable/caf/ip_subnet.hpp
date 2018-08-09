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
#include <caf/ip_subnet.hpp>
#include <caf/ipv4_address.hpp>
#include <caf/ipv4_subnet.hpp>

#include "vast/concept/parseable/caf/ip_address.hpp"
#include "vast/concept/parseable/caf/ipv4_address.hpp"
#include "vast/concept/parseable/core/parser.hpp"
#include "vast/concept/parseable/numeric/integral.hpp"

namespace vast {

template <>
struct access::parser<caf::ip_subnet>
  : vast::parser<access::parser<caf::ip_subnet>> {
  using attribute = caf::ip_subnet;

  static auto make() {
    using namespace parsers;
    auto prefix = u8.with([](auto x) { return x <= 128; });
    return addr >> '/' >> prefix;
  }

  static auto make_v4() {
    using namespace parsers;
    auto prefix = u8.with([](auto x) { return x <= 32; });
    return v4_addr >> '/' >> prefix;
  }

  template <class Iterator>
  bool parse(Iterator& f, const Iterator& l, attribute& a) const {
    // Always try v4 first, because the prefix length otherwise can get messed
    // up. For example, the IPv6 parser happily accepts '1.2.3.4', but then we
    // don't know how to interpret a following `/8`. Asking the address for
    // `empbeds_v4()` is prone to error, because the input could as well state
    // the v4 prefix manually: `64:ff9b::1.2.3.4/8` (which we want to interpret
    // differently than `1.2.3.4/8`).
    uint8_t len;
    caf::ipv4_address v4_tmp;
    static auto parse_v4 = make_v4();
    if (parse_v4(f, l, v4_tmp, len)) {
      a = caf::ipv6_subnet{v4_tmp, len};
      return true;
    }
    caf::ip_address tmp;
    static auto parse_ip = make();
    if (parse_ip(f, l, tmp, len)) {
      a = attribute{tmp, len};
      return true;
    }
    return false;
  }

  template <class Iterator>
  bool parse(Iterator& f, const Iterator& l, unused_type) const {
    static auto p = make();
    return p(f, l, unused);
  }
};

template <>
struct parser_registry<caf::ip_subnet> {
  using type = access::parser<caf::ip_subnet>;
};

namespace parsers {

static auto const net = make_parser<caf::ip_subnet>();

} // namespace parsers

} // namespace vast

