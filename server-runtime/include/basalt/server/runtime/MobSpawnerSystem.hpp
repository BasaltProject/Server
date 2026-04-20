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

class MobSpawnerSystem {
public:
    struct Options {
        std::function<std::int64_t()> nextSeed;
        world::SpawnRuleFacts facts;
        world::RealLootTables* lootTables = nullptr;
    };

    MobSpawnerSystem(player::PlayerRegistry& players, NaturalSpawningSystem& spawning, Options options);

    bool tick(world::LoadedWorld& world, wire::nbt::NbtCompound& data, const std::array<std::int32_t, 3>& pos,
              const std::function<void(std::int32_t eventId)>& broadcastEvent,
              const std::function<void()>& spawnDataChanged = {});

private:
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

    [[nodiscard]] bool isNearPlayer(const world::LoadedWorld& world, const std::array<std::int32_t, 3>& pos,
                                    std::int32_t range) const;

    void delay(Turn& turn) const;

    [[nodiscard]] const world::SpawnData& nextSpawnData(Turn& turn) const;

    [[nodiscard]] Attempt attempt(Turn& turn, const world::SpawnData& spawn);

    void broadcastSpawned(world::LoadedWorld& world, const std::array<std::int32_t, 3>& pos, std::int32_t blockX,
                          std::int32_t blockY, std::int32_t blockZ, const wire::common::Uuid& source,
                          std::optional<std::int32_t> animatedMob) const;

    void equip(world::LoadedWorld& world, wire::nbt::NbtCompound& mob, const world::EquipmentTable& table, double x,
               double y, double z, random::RandomSource& random) const;

    [[nodiscard]] std::vector<entity::Aabb> playerBoxes(const world::LoadedWorld& world) const;

    [[nodiscard]] std::vector<player::PlayerSession*> playersIn(const world::LoadedWorld& world) const;

    player::PlayerRegistry* players_;
    NaturalSpawningSystem* spawning_;
    Options options_;
};

} // namespace basalt::server::runtime
