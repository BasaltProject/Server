#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/entity/Aabb.hpp>
#include <basalt/server/runtime/world/BaseSpawnerData.hpp>
#include <basalt/server/runtime/world/SpawnPlacementRules.hpp>
#include <basalt/wire/common/Uuid.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
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

class NaturalSpawningSystem;

/// The monster spawner: a cage that makes mobs while a player is near.
///
/// parity: `BaseSpawner.serverTick`, which both the spawner block and the spawner minecart run. The
/// state lives in the caller's NBT (`SpawnerBlockEntity`'s data, or `MinecartSpawner`'s saved
/// entity), so the one system serves both and keeps nothing of its own between ticks.
class MobSpawnerSystem {
public:
    struct Options {
        /// A seed for the level's stream, drawn once per spawner turn that does anything.
        std::function<std::int64_t()> nextSeed;
        /// The tags and the players the spawn rules ask about.
        world::SpawnRuleFacts facts;
        /// Where an `equipment` table is looked up. Null rolls nothing, which is a server without a jar.
        world::RealLootTables* lootTables = nullptr;
    };

    MobSpawnerSystem(player::PlayerRegistry& players, NaturalSpawningSystem& spawning, Options options);

    /// One turn of `BaseSpawner.serverTick` for the spawner saved in `data` at `pos` (the block, or the
    /// minecart's block position). Answers whether `data` changed, so the caller saves it.
    /// `broadcastEvent` is `BaseSpawner.broadcastEvent` (only id 1, the countdown reset);
    /// `spawnDataChanged` is `setNextSpawnData`, which makes the block resend its block entity.
    bool tick(world::LoadedWorld& world, wire::nbt::NbtCompound& data, const std::array<std::int32_t, 3>& pos,
              const std::function<void(std::int32_t eventId)>& broadcastEvent,
              const std::function<void()>& spawnDataChanged = {});

private:
    /// How one spawn attempt ended. parity: the three exits of the loop body in `serverTick`.
    enum class Attempt : std::uint8_t { Skipped, Spawned, Abort };

    struct Turn {
        world::LoadedWorld& world;
        world::BaseSpawnerState& state;
        const std::array<std::int32_t, 3>& pos;
        random::RandomSource& random;
        const std::function<void(std::int32_t)>& broadcastEvent;
        const std::function<void()>& spawnDataChanged;
        bool dataChanged = false;
    };

    /// parity: `BaseSpawner.isNearPlayer` over `EntityGetter.hasNearbyAlivePlayer`.
    [[nodiscard]] bool isNearPlayer(const world::LoadedWorld& world, const std::array<std::int32_t, 3>& pos,
                                    std::int32_t range) const;

    /// parity: `BaseSpawner.delay`.
    void delay(Turn& turn) const;

    /// parity: `BaseSpawner.getOrCreateNextSpawnData`.
    [[nodiscard]] const world::SpawnData& nextSpawnData(Turn& turn) const;

    /// One pass of the loop in `serverTick`: position, rules, the entity, the checks, the world.
    [[nodiscard]] Attempt attempt(Turn& turn, const world::SpawnData& spawn);

    /// What a spawn tells the world: the cage's particles, the vibration, and the mob's puff when
    /// `animatedMob` names one.
    void broadcastSpawned(world::LoadedWorld& world, const std::array<std::int32_t, 3>& pos, std::int32_t blockX,
                          std::int32_t blockY, std::int32_t blockZ, const wire::common::Uuid& source,
                          std::optional<std::int32_t> animatedMob) const;

    /// parity: `Mob.equip(EquipmentTable)` onto the saved mob.
    void equip(world::LoadedWorld& world, wire::nbt::NbtCompound& mob, const world::EquipmentTable& table, double x,
               double y, double z, random::RandomSource& random) const;

    /// The boxes of the players in this world, alive or not, that are not spectating.
    [[nodiscard]] std::vector<entity::Aabb> playerBoxes(const world::LoadedWorld& world) const;

    [[nodiscard]] std::vector<player::PlayerSession*> playersIn(const world::LoadedWorld& world) const;

    player::PlayerRegistry* players_;
    NaturalSpawningSystem* spawning_;
    Options options_;
};

} // namespace basalt::server::runtime
