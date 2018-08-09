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

#include <caf/ip_subnet.hpp>
#include <caf/ipv4_subnet.hpp>

#include "vast/concept/printable/caf/ip_address.hpp"
#include "vast/concept/printable/caf/ip_subnet.hpp"
#include "vast/concept/printable/core.hpp"
#include "vast/concept/printable/numeric/integral.hpp"
#include "vast/concept/printable/string/char.hpp"

namespace vast {

struct subnet_printer : printer<subnet_printer> {
  using attribute = caf::ip_subnet;

  template <class Iterator>
  bool print(Iterator& out, const attribute& sn) const {
    using namespace printers;
    if (sn.embeds_v4()) {
      auto v4 = sn.embedded_v4();
      return (addr << chr<'/'> << u8)(out, sn.network_address(),
                                      v4.prefix_length());
    }
    return (addr << chr<'/'> << u8)(out, sn.network_address(),
                                    sn.prefix_length());
  }
};

template <>
struct printer_registry<caf::ip_subnet> {
  using type = subnet_printer;
};

} // namespace vast

