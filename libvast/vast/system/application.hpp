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

#include <memory>
#include <string>
#include <string_view>

#include "vast/command.hpp"
#include "vast/system/configuration.hpp"

namespace vast::system {

class application {
public:
  /// Constructs an application.
  /// @param cfg The VAST system configuration.
  application();

  /// Adds a new command to the application.
  auto add(command::fun run, std::string_view name,
           std::string_view description, caf::config_option_set options = {}) {
    return root.add(run, name, description, std::move(options));
  }

  /// Starts the application and blocks until execution completes.
  /// @returns the type-erased result of the executed command or a wrapped
  ///          `caf::error`.
  auto run(caf::actor_system& sys, command::argument_iterator first,
           command::argument_iterator last) {
    return vast::run(root, sys, first, last);
  }

  /// The entry point for command dispatching.
  command root;
};

} // namespace vast::system

