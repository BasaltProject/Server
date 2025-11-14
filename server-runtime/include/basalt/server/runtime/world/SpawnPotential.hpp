#pragma once

#include <cstdint>
#include <vector>

namespace basalt::server::runtime::world {

/// How strongly one mob repels another of the same biome-defined cost family.
struct MobSpawnCost {
    double energyBudget = 0.0;
    double charge = 0.0;

    friend bool operator==(const MobSpawnCost&, const MobSpawnCost&) = default;
};

/// The biome-defined repulsion field that keeps costly mobs from packing into one Nether pocket.
///
/// parity: `PotentialCalculator`: every existing charge contributes `charge / distance`; a proposed
/// mob multiplies that field by its own charge and compares it to its biome's energy budget.
class SpawnPotential {
public:
    void add(std::int32_t x, std::int32_t y, std::int32_t z, double charge);

    [[nodiscard]] bool allows(std::int32_t x, std::int32_t y, std::int32_t z, const MobSpawnCost& cost) const;

    /// What adding a charge here would cost, for the caller that wants the number rather than the
    /// verdict.
    [[nodiscard]] double energyChange(std::int32_t x, std::int32_t y, std::int32_t z, double charge) const;

    [[nodiscard]] std::size_t size() const noexcept { return charges_.size(); }

private:
    struct PointCharge {
        std::int32_t x = 0;
        std::int32_t y = 0;
        std::int32_t z = 0;
        double charge = 0.0;
    };

    std::vector<PointCharge> charges_;
};

} // namespace basalt::server::runtime::world
