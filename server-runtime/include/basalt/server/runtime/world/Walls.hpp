#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>

namespace basalt::server::runtime::world {

/// parity: `WallBlock.updateShape`.
namespace walls {

[[nodiscard]] std::optional<std::map<std::string, std::string>> shapeOf(std::int32_t stateId, std::int32_t north,
                                                                        std::int32_t south, std::int32_t west,
                                                                        std::int32_t east, std::int32_t above,
                                                                        bool aboveOverridesPost = false);

} // namespace walls

} // namespace basalt::server::runtime::world
