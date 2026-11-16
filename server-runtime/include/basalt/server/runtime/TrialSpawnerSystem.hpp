#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/TrialSpawners.hpp>
#include <basalt/server/world/loot/LootContext.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace basalt::server::runtime::player {
class PlayerRegistry;
class PlayerSession;
} // namespace basalt::server::runtime::player

namespace basalt::server::runtime::world {
class LoadedWorld;
class RealLootTables;
class RealTrialSpawnerConfigs;
} // namespace basalt::server::runtime::world

namespace basalt::server::runtime {

class BlockChangeBroadcaster;
class MobSpawnerSystem;
class OminousItemSpawnerSystem;

/// parity: `TrialSpawner.tickServer` with `TrialSpawnerState.tickAndGetNext`.
class TrialSpawnerSystem {
public:
    struct Options {
        std::function<std::int64_t()> nextSeed;
        const world::RealTrialSpawnerConfigs* configs = nullptr;
        world::RealLootTables* lootTables = nullptr;
    };

    TrialSpawnerSystem(player::PlayerRegistry& players, BlockChangeBroadcaster& broadcaster, MobSpawnerSystem& mobs,
                       OminousItemSpawnerSystem& ominousItems, Options options);

    bool tick(world::LoadedWorld& world, const std::array<std::int32_t, 3>& pos);

private:
    struct Turn;

    [[nodiscard]] world::trialSpawners::State step(Turn& turn, world::trialSpawners::State current);

    void detectPlayers(Turn& turn, world::trialSpawners::State current);

    [[nodiscard]] bool hasMobToSpawn(Turn& turn) const;

    [[nodiscard]] std::vector<wire::common::Uuid> playersInRange(Turn& turn, bool requireSight) const;

    void becomeOminous(Turn& turn, player::PlayerSession& trigger);

    void ejectReward(Turn& turn, const std::string& table);

    using Dispensing = std::vector<std::pair<server::world::loot::ItemStack, std::int32_t>>;

    [[nodiscard]] const Dispensing& dispensingItems(Turn& turn);

    void spawnOminousItemSpawner(Turn& turn);

    [[nodiscard]] std::optional<std::array<double, 3>> positionToSpawnSpawner(Turn& turn);

    void soundAt(Turn& turn, const char* name, wire::protocol::play::SoundSource source, double x, double y, double z,
                 float volume, float pitch) const;

    void setBlock(Turn& turn, const char* property, const std::string& value);

    void levelEvent(Turn& turn, std::int32_t type, const std::array<std::int32_t, 3>& at, std::int32_t data) const;

    void sound(Turn& turn, const char* name) const;

    [[nodiscard]] bool canSpawnInLevel(const Turn& turn) const;

    player::PlayerRegistry* players_;
    BlockChangeBroadcaster* broadcaster_;
    MobSpawnerSystem* mobs_;
    OminousItemSpawnerSystem* ominousItems_;
    Options options_;
    std::map<std::pair<std::string, std::array<std::int32_t, 3>>, Dispensing> dispensing_;
};

} // namespace basalt::server::runtime
