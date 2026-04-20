#pragma once

#include <basalt/server/runtime/world/WorldState.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace basalt::server::runtime::entity {

namespace swimPath {

inline constexpr std::int32_t kMaxNodes = 256;

[[nodiscard]] bool holdsWater(std::int32_t stateId);

[[nodiscard]] bool isSwimmable(world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

[[nodiscard]] bool isBreach(world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

[[nodiscard]] std::optional<std::vector<std::array<std::int32_t, 3>>> find(world::WorldState& world,
                                                                           const std::array<std::int32_t, 3>& from,
                                                                           const std::array<std::int32_t, 3>& to,
                                                                           bool breaching);

} // namespace swimPath

} // namespace basalt::server::runtime::entity
