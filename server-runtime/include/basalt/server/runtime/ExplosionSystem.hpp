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

/// Everything an explosion owes the world and the clients watching it: what it breaks, what that
/// drops, and who it hurts.
class ExplosionSystem {
public:
    /// Whether an explosion may go off at all, for a plugin that lets creepers off and stops TNT. A
    /// callback rather than the plugin bus: unset means nobody objects.
    struct Hooks {
        std::function<bool(const world::LoadedWorld& world, double x, double y, double z, float power,
                           std::string_view cause, std::int32_t sourceEntityId)>
            allowed;
        /// Hurts the entities around the blast that this system does not own. parity: the rest of
        /// `ServerExplosion.hurtEntities`.
        std::function<void(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers, double x,
                           double y, double z, float power, std::int32_t sourceEntityId)>
            struck;
    };

    /// What the blast rolls its loot and scatters its drops with.
    struct Options {
        std::function<std::int64_t()> nextSeed;
        std::function<double()> nextDouble;
    };

    ExplosionSystem(player::PlayerRegistry& players, world::RealLootTables& lootTables, Hooks hooks, Options options);

    [[nodiscard]] static std::int32_t particleId(std::string_view path);

    /// Sets off an explosion, drops what it broke and hurts whoever was standing in it.
    ///
    /// parity: `ServerExplosion`, the same shape as the bed's own: what it breaks drops with no tool
    /// involved, and the neighbours of every hole are told afterwards.
    ///
    /// `sourceEntityId` is 0 for a blast with nothing behind it, which is what a bed in the Nether
    /// and a command-driven explosion both are.
    /// `spares` is what the source entity refuses to break, and only a primed tnt cart has one.
    void blowUp(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers, double x, double y,
                double z, float power, std::string_view cause = "explosion", std::int32_t sourceEntityId = 0,
                const world::explosions::Spares& spares = {}, const world::explosions::Resistance& resistance = {});

private:
    /// One stack the blast is holding on to until it decides where it lands.
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

    /// parity: `Mth.nextDouble(random, -0.25, 0.25)`, the quarter block a popped stack lands within.
    [[nodiscard]] double jitter() const;

    player::PlayerRegistry* players_;
    world::RealLootTables* lootTables_;
    Hooks hooks_;
    Options options_;
};

} // namespace basalt::server::runtime
