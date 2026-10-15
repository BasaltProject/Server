#include <basalt/server/runtime/world/SpawnPotential.hpp>

#include <cmath>
#include <cstdint>
#include <limits>

namespace basalt::server::runtime::world {

void SpawnPotential::add(std::int32_t x, std::int32_t y, std::int32_t z, double charge) {
    // A charge of nothing repels nothing, and every mob that has no cost in this biome has one.
    if (charge != 0.0) {
        charges_.push_back(PointCharge{.x = x, .y = y, .z = z, .charge = charge});
    }
}

bool SpawnPotential::allows(std::int32_t x, std::int32_t y, std::int32_t z, const MobSpawnCost& cost) const {
    return energyChange(x, y, z, cost.charge) <= cost.energyBudget;
}

double SpawnPotential::energyChange(std::int32_t x, std::int32_t y, std::int32_t z, double charge) const {
    if (charge == 0.0) {
        return 0.0;
    }
    double potential = 0.0;
    for (const PointCharge& point : charges_) {
        const double dx = static_cast<double>(point.x - x);
        const double dy = static_cast<double>(point.y - y);
        const double dz = static_cast<double>(point.z - z);
        const double distanceSquared = dx * dx + dy * dy + dz * dz;
        if (distanceSquared == 0.0) {
            return std::numeric_limits<double>::infinity();
        }
        potential += point.charge / std::sqrt(distanceSquared);
    }
    return potential * charge;
}

} // namespace basalt::server::runtime::world
