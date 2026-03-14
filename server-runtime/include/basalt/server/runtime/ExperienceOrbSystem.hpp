#pragma once

#include <basalt/server/random/RandomSource.hpp>

#include <cstdint>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace basalt::server::runtime::player {
class PlayerRegistry;
class PlayerSession;
} // namespace basalt::server::runtime::player

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime::entity {
class ServerEntity;
}

namespace basalt::server::runtime {

/// Experience lying about as orbs: where it comes from, how it drifts to a player and what it gives.
///
/// parity: `ExperienceOrb`. The value, count, age and health live in the entity's tag under vanilla's
/// keys, which is also how vanilla saves them.
class ExperienceOrbSystem {
public:
    /// parity: `DATA_VALUE`.
    static constexpr std::uint8_t kValueDataIndex = 8;

    struct Options {
        std::function<std::int64_t()> nextSeed;
    };

    ExperienceOrbSystem(player::PlayerRegistry& players, Options options);

    [[nodiscard]] static bool isOrb(const entity::ServerEntity& entity);

    [[nodiscard]] static std::int32_t valueOf(const entity::ServerEntity& orb);

    /// parity: `ExperienceOrb.award`: the amount split into vanilla's sizes, each one joining an orb
    /// nearby of the same size when the dice allow.
    void award(world::LoadedWorld& world, double x, double y, double z, std::int32_t amount);

    /// One orb of exactly this value, never merged. parity: `new ExperienceOrb(level, x, y, z, value)`.
    void spawn(world::LoadedWorld& world, double x, double y, double z, std::int32_t value);

    void tick(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
              const std::vector<entity::ServerEntity*>& living, entity::ServerEntity& orb);

    /// Takes damage off an orb. parity: `ExperienceOrb.hurtServer`.
    static void hurt(entity::ServerEntity& orb, float damage);

    /// Counts every player's pickup delay down. parity: `Player.takeXpDelay`.
    void tickPlayers(const std::vector<player::PlayerSession*>& players);

private:
    void spawnLocked(world::LoadedWorld& world, double x, double y, double z, std::int32_t value);

    player::PlayerRegistry* players_;
    Options options_;
    random::RandomSource random_;
    std::mutex lock_;
    std::unordered_map<std::int32_t, std::int32_t> takeDelays_;
};

} // namespace basalt::server::runtime
