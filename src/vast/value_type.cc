#include "vast/value_type.h"

#include "vast/logger.h"
#include "vast/serialization.h"

namespace vast {

void serialize(serializer& sink, value_type x)
{
  VAST_ENTER("value_type: " << VAST_ARG(x));
  sink << static_cast<std::underlying_type<value_type>::type>(x);
}

void deserialize(deserializer& source, value_type& x)
{
  VAST_ENTER();
  std::underlying_type<value_type>::type u;
  source >> u;
  x = static_cast<value_type>(u);
  VAST_LEAVE("value_type: " << x);
}

} // namespace vast
