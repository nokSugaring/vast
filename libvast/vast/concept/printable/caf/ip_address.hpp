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

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <cstring>

#include <caf/ip_address.hpp>

#include "vast/access.hpp"
#include "vast/concept/printable/core/printer.hpp"
#include "vast/concept/printable/string/string.hpp"

namespace vast {

template <>
struct access::printer<caf::ip_address>
  : vast::printer<access::printer<caf::ip_address>> {
  using attribute = caf::ip_address;

  template <class Iterator>
  bool print(Iterator& out, const attribute& a) const {
    return printers::str.print(out, to_string(a));
  }
};

template <>
struct printer_registry<caf::ip_address> {
  using type = access::printer<caf::ip_address>;
};

namespace printers {

auto const addr = make_printer<caf::ip_address>{};

} // namespace printers
} // namespace vast

