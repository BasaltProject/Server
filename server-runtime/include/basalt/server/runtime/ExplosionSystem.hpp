#pragma once

#include <basalt/server/runtime/world/Explosions.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::player {
class PlayerRegistry;
class PlayerSession;
} // namespace basalt::server::runtime::player

namespace basalt::server::runtime::world {
class LoadedWorld;
class RealLootTables;
} // namespace basalt::server::runtime::world

namespace basalt::server::runtime {

class ExplosionSystem {
public:
    struct Hooks {
        std::function<bool(const world::LoadedWorld& world, double x, double y, double z, float power,
                           std::string_view cause, std::int32_t sourceEntityId)>
            allowed;
        /// Hurts the entities around the blast that this system does not own. parity: the rest of
        std::function<void(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers, double x,
                           double y, double z, float power, std::int32_t sourceEntityId)>
            struck;
        std::function<void(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                           std::int32_t stateId, const wire::nbt::NbtCompound& hive, std::int32_t sourceEntityId)>
            hiveBlown;
    };

    struct Options {
        std::function<std::int64_t()> nextSeed;
        std::function<double()> nextDouble;
    };

    ExplosionSystem(player::PlayerRegistry& players, world::RealLootTables& lootTables, Hooks hooks, Options options);

    [[nodiscard]] static std::int32_t particleId(std::string_view path);

    /// parity: `ServerExplosion`.
    void blowUp(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers, double x, double y,
                double z, float power, std::string_view cause = "explosion", std::int32_t sourceEntityId = 0,
                const world::explosions::Spares& spares = {}, const world::explosions::Resistance& resistance = {},
                bool fire = false);

private:
    struct Drop {
        wire::common::Identifier item;
        std::int32_t count = 0;
        std::int32_t x = 0;
        std::int32_t y = 0;
        std::int32_t z = 0;
    };

    void collectExplodedBlock(world::LoadedWorld& world, const wire::common::Identifier& block, std::int32_t stateId,
                              std::int32_t x, std::int32_t y, std::int32_t z, float power,
                              std::vector<Drop>& drops) const;

    void popResource(world::LoadedWorld& world, const Drop& drop) const;

    [[nodiscard]] double jitter() const;

    player::PlayerRegistry* players_;
    world::RealLootTables* lootTables_;
    Hooks hooks_;
    Options options_;
};

} // namespace basalt::server::runtime
