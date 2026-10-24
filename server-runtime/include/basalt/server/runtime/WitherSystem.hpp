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
class AdvancementTriggers;
class LoadedWorld;
} // namespace basalt::server::runtime::world

namespace basalt::server::runtime {

class BlockChangeBroadcaster;
class BossBarView;
class ExplosionSystem;

class WitherSystem {
public:
    struct Options {
        std::function<std::int64_t()> nextSeed;
        const world::realBlockTags::TagMembers* blockTags = nullptr;
        world::AdvancementTriggers* advancementTriggers = nullptr;
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

    [[nodiscard]] static entity::witherBoss::WitherState& stateOf(entity::ServerEntity& wither);

    void placed(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z);

    /// parity: the part of `WitherBoss.aiStep` before `super.aiStep`.
    void beforeTurn(world::LoadedWorld& world, const std::vector<entity::ServerEntity*>& living,
                    entity::ServerEntity& wither);

    void afterTurn(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                   const std::vector<entity::ServerEntity*>& living, entity::ServerEntity& wither);

    void shootAtTarget(world::LoadedWorld& world, entity::ServerEntity& wither);

    void skullLanded(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                     entity::ServerEntity& skull, entity::ServerEntity* mob, player::PlayerSession* player);

    /// parity: `WitherBoss.dropCustomDeathLoot`, the star that never despawns.
    void dropStar(world::LoadedWorld& world, const entity::ServerEntity& wither);

    void keepBar(world::LoadedWorld& world, entity::ServerEntity& wither);

    /// Runs `use` on the bar of this wither, if it has one yet.
    bool withBar(std::int32_t entityId, const std::function<void(BossBarView&, player::PlayerRegistry&)>& use);

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
