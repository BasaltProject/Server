#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/loot/LootContext.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>

namespace basalt::server::runtime::player {
class PlayerRegistry;
} // namespace basalt::server::runtime::player

namespace basalt::server::runtime::world {
class LoadedWorld;
} // namespace basalt::server::runtime::world

namespace basalt::server::runtime::entity {
class ServerEntity;
} // namespace basalt::server::runtime::entity

namespace basalt::server::runtime {

/// parity: `OminousItemSpawner`, the entity a trial spawner drops on its targets.
class OminousItemSpawnerSystem {
public:
    struct Options {
        std::function<std::int64_t()> nextSeed;
    };

    OminousItemSpawnerSystem(player::PlayerRegistry& players, Options options);

    [[nodiscard]] static bool isSpawner(const entity::ServerEntity& entity);

    /// parity: `OminousItemSpawner.create`.
    entity::ServerEntity* spawn(world::LoadedWorld& world, const server::world::loot::ItemStack& stack, double x,
                                double y, double z, random::RandomSource& random);

    void tick(world::LoadedWorld& world, entity::ServerEntity& spawner, random::RandomSource& random);

private:
    void spawnItem(world::LoadedWorld& world, const entity::ServerEntity& spawner,
                   const server::world::loot::ItemStack& stack, random::RandomSource& random);

    void levelEvent(world::LoadedWorld& world, std::int32_t type, const std::array<std::int32_t, 3>& at,
                    std::int32_t data) const;

    player::PlayerRegistry* players_;
    Options options_;
};

} // namespace basalt::server::runtime
