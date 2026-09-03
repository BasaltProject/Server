#pragma once

#include <cstdint>

namespace basalt::server::runtime::world {

class WorldState;

/// Village-section queries shared by custom spawners until the runtime owns a full POI claim map.
namespace villageProximity {

/// parity: `PoiManager.sectionsToVillage`. With no POI-claim table yet, a live villager's section
/// stands in for an occupied POI; the distance is still `SectionTracker`'s Chebyshev.
[[nodiscard]] bool isCloseToVillage(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                    std::int32_t sectionDistance);

/// One live villager stands in for one claimed home until claim tickets exist. The radius is
/// `PoiManager.getCountInRange`'s spherical one, not the section metric above.
[[nodiscard]] std::int32_t occupiedHomesNear(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                             std::int32_t radius);

} // namespace villageProximity

} // namespace basalt::server::runtime::world
