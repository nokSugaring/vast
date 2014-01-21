#ifndef VAST_INDIVIDUAL_H
#define VAST_INDIVIDUAL_H

#include "vast/uuid.h"
#include "vast/fwd.h"
#include "vast/util/operators.h"

namespace vast {

/// An object with a unique ID.
class individual : util::totally_ordered<individual>
{
public:
  /// Constructs an object with a random ID.
  /// @param id The instance's UUID.
  individual(uuid id = uuid::random());

  /// Retrieves the individual's ID.
  /// @returns A UUID describing the instance.
  uuid const& id() const;

  /// Sets the individual's ID.
  /// @param id A UUID describing the instance.
  void id(uuid id);

private:
  uuid id_;

protected:
  friend access;
  void serialize(serializer& sink) const;
  void deserialize(deserializer& source);

  friend bool operator<(individual const& x, individual const& y);
  friend bool operator==(individual const& x, individual const& y);
};

} // namespace vast

#endif
