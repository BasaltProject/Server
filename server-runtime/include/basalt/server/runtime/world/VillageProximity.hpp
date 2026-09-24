#pragma once

#include <cstdint>

namespace basalt::server::runtime::world {

class WorldState;

/// Village-section queries shared by the custom spawners, answered by the world's points of interest.
namespace villageProximity {

/// parity: `ServerLevel.isCloseToVillage` over `PoiManager.sectionsToVillage`.
[[nodiscard]] bool isCloseToVillage(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                    std::int32_t sectionDistance);

/// parity: `PoiManager.getCountInRange` of the homes somebody has claimed.
[[nodiscard]] std::int32_t occupiedHomesNear(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                             std::int32_t radius);

} // namespace villageProximity

} // namespace basalt::server::runtime::world
