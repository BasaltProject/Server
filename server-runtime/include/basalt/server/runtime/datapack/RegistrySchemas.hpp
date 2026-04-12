#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace basalt::wire::common {
class JsonObject;
}

namespace basalt::server::runtime::datapack {

/// Why one element does not fit its registry's codec, or nothing when it does or when this build
/// has no schema for the registry. Only the fields the codec requires are judged, and their kind,
/// so a valid element is never refused. parity: the `DIRECT_CODEC` of each registry.
[[nodiscard]] std::optional<std::string> schemaError(std::string_view registry,
                                                     const wire::common::JsonObject& element);

} // namespace basalt::server::runtime::datapack
