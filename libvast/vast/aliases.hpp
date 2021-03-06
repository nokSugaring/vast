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

#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <caf/fwd.hpp>
#include <caf/dictionary.hpp>
#include <caf/config_value.hpp>

#include "vast/fwd.hpp"

#include "vast/detail/steady_map.hpp"
#include "vast/detail/steady_set.hpp"

namespace vast {

/// Boolean artithmetic type.
using boolean = bool;

/// Signed integer type.
using integer = int64_t;

/// Unsigned integer type.
using count = uint64_t;

/// Floating point type.
using real = double;

/// Enumeration type.
using enumeration = count;

/// A random-access sequence of data.
using vector = std::vector<data>;

/// A mathematical set where each element is ::data.
using set = detail::steady_set<data>;

/// An associative array with ::data as both key and value.
using map = detail::steady_map<data, data>;

/// Default bitstream implementation.
using default_bitstream = ewah_bitstream;

/// Uniquely identifies a VAST event.
using id = uint64_t;

/// The ID for invalid events
constexpr id invalid_id = std::numeric_limits<id>::max();

/// The largest possible event ID.
constexpr id max_id = invalid_id - 1;

/// The largest number of representable events.
constexpr id max_events = max_id + 1;

/// Iterates over CLI arguments.
using cli_argument_iterator = std::vector<std::string>::const_iterator;

/// Convenience alias for function return types that either return an actor or
/// an error.
using maybe_actor = caf::expected<caf::actor>;

/// Additional runtime information to pass to the synopsis factory.
using synopsis_options = caf::dictionary<caf::config_value>;

/// Bundles an offset into an expression under evaluation to the curried
/// representation of the ::predicate at that position in the expression and
/// the INDEXER actor responsible for answering the (curried) predicate.
using evaluation_triple = std::tuple<offset, curried_predicate, caf::actor>;

/// Maps layouts to a list of evaluation triples.
using evaluation_map = std::map<type, std::vector<evaluation_triple>>;

} // namespace vast
