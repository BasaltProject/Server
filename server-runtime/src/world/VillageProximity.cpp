#include <basalt/server/runtime/world/VillageProximity.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/server/runtime/world/poi/PoiManager.hpp>

#include <cstdint>

namespace basalt::server::runtime::world::villageProximity {

bool isCloseToVillage(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                      std::int32_t sectionDistance) {
    return world.poi().isCloseToVillage(poi::BlockPos{x, y, z}, sectionDistance);
}

std::int32_t occupiedHomesNear(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                               std::int32_t radius) {
    return static_cast<std::int32_t>(world.poi().countInRange(poi::maskOf(poi::PoiType::kHome), poi::BlockPos{x, y, z},
                                                              radius, poi::Occupancy::kIsOccupied));
}

} // namespace basalt::server::runtime::world::villageProximity
