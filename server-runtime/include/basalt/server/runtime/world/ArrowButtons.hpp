#pragma once

#include <basalt/server/runtime/world/WorldState.hpp>

#include <array>
#include <cstdint>

namespace basalt::server::runtime::world {

namespace arrowButtons {

[[nodiscard]] bool acceptsArrows(std::string_view path);

[[nodiscard]] std::array<double, 6> outline(std::int32_t stateId);

[[nodiscard]] bool reaches(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId, double entityX,
                           double entityY, double entityZ, double halfWidth, double height);

[[nodiscard]] bool heldByArrow(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId);

} // namespace arrowButtons

} // namespace basalt::server::runtime::world
