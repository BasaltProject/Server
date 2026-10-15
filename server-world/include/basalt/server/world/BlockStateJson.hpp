#pragma once

#include <cstdint>
#include <optional>

namespace basalt::wire::common {
class JsonValue;
}

namespace basalt::server::world::blockStateJson {

/// parity: `BlockState.CODEC` since 26.3.
[[nodiscard]] std::optional<std::int32_t> parse(const wire::common::JsonValue& json);

} // namespace basalt::server::world::blockStateJson
