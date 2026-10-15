#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace basalt::wire::common {
class JsonObject;
}

namespace basalt::server::runtime::datapack {

/// parity: the `DIRECT_CODEC` of each registry.
[[nodiscard]] std::optional<std::string> schemaError(std::string_view registry,
                                                     const wire::common::JsonObject& element);

} // namespace basalt::server::runtime::datapack
