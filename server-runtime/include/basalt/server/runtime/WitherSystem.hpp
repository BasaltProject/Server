#pragma once

#include <basalt/server/runtime/entity/WitherBoss.hpp>
#include <basalt/server/runtime/world/MobEffects.hpp>
#include <basalt/server/runtime/world/RealBlockTags.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace basalt::server::runtime::entity {
class ServerEntity;
}

namespace basalt::server::runtime::player {
class PlayerRegistry;
class PlayerSession;
} // namespace basalt::server::runtime::player

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime {

class BlockChangeBroadcaster;
class BossBarView;
class ExplosionSystem;

/// The wither's reach into the world: how it is built, what its heads shoot, the blocks it breaks
/// and the bar its watchers see.
///
/// parity: `WitherSkullBlock.checkSpawn`, the world half of `WitherBoss` and `WitherSkull.onHit`.
class WitherSystem {
public:
    struct Options {
        std::function<std::int64_t()> nextSeed;
        /// `wither_summon_base_blocks` and `wither_immune`.
        const world::realBlockTags::TagMembers* blockTags = nullptr;
        /// A status effect given to a mob.
        std::function<bool(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                           entity::ServerEntity& mob, const world::ActiveEffect& effect)>
            mobEffect;
    };

    WitherSystem(player::PlayerRegistry& players, BlockChangeBroadcaster& broadcaster, ExplosionSystem& explosions,
                 Options options);
    ~WitherSystem();

    WitherSystem(const WitherSystem&) = delete;
    WitherSystem& operator=(const WitherSystem&) = delete;

    [[nodiscard]] static bool isWither(const entity::ServerEntity& entity);

    /// The state of a wither, made on first use for one that came from a command or the disk.
    [[nodiscard]] static entity::witherBoss::WitherState& stateOf(entity::ServerEntity& wither);

    /// A block just went down by hand there. parity: `WitherSkullBlock.setPlacedBy`.
    void placed(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z);

    /// parity: the part of `WitherBoss.aiStep` before `super.aiStep`: rising to the target and
    /// following it.
    void beforeTurn(world::LoadedWorld& world, const std::vector<entity::ServerEntity*>& living,
                    entity::ServerEntity& wither);

    /// parity: `WitherBoss.customServerAiStep`.
    void afterTurn(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                   const std::vector<entity::ServerEntity*>& living, entity::ServerEntity& wither);

    /// The middle head's shot at its target. parity: `performRangedAttack(target, power)`.
    void shootAtTarget(world::LoadedWorld& world, entity::ServerEntity& wither);

    /// parity: `WitherSkull.onHitEntity` and `onHit`. `mob` or `player` is who it struck, if anyone.
    void skullLanded(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                     entity::ServerEntity& skull, entity::ServerEntity* mob, player::PlayerSession* player);

    /// parity: `WitherBoss.dropCustomDeathLoot`, the star that never despawns.
    void dropStar(world::LoadedWorld& world, const entity::ServerEntity& wither);

    /// Takes the bars of withers that took no turn since the last call from whoever saw them.
    void sweepBars();

private:
    struct Aim {
        std::int32_t entityId = 0;
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
        double height = 0.0;
        bool alive = false;
    };

    [[nodiscard]] std::optional<Aim> aimAt(world::LoadedWorld& world, std::int32_t entityId) const;

    [[nodiscard]] bool tagged(std::int32_t stateId, std::string_view tag) const;

    void shoot(world::LoadedWorld& world, entity::ServerEntity& wither, std::int32_t head, double x, double y, double z,
               bool dangerous);

    void tickHeads(world::LoadedWorld& world, const std::vector<entity::ServerEntity*>& living,
                   entity::ServerEntity& wither, entity::witherBoss::WitherState& state);

    void destroyBlocks(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                       entity::ServerEntity& wither);

    void syncData(const std::vector<player::PlayerSession*>& watchers, const entity::ServerEntity& wither,
                  std::uint8_t index, std::int32_t value) const;

    void levelEvent(const std::vector<player::PlayerSession*>& watchers, std::int32_t event,
                    const entity::ServerEntity& at) const;

    void updateBar(const std::vector<player::PlayerSession*>& watchers, entity::ServerEntity& wither, float progress);

    [[nodiscard]] std::vector<player::PlayerSession*> watchersOf(const world::LoadedWorld& world) const;

    player::PlayerRegistry* players_;
    BlockChangeBroadcaster* broadcaster_;
    ExplosionSystem* explosions_;
    Options options_;

    struct Bar {
        std::shared_ptr<BossBarView> view;
        bool seen = false;
    };

    std::mutex barsLock_;
    std::map<std::int32_t, Bar> bars_;
};

} // namespace basalt::server::runtime
