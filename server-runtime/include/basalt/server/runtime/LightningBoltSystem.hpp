#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace basalt::server::random {
class RandomSource;
}

namespace basalt::server::runtime::entity {
class ServerEntity;
}

namespace basalt::server::runtime::player {
class PlayerSession;
}

namespace basalt::server::runtime::world {
class AdvancementTriggers;
class LoadedWorld;
} // namespace basalt::server::runtime::world

namespace basalt::server::runtime {

class BlockChangeBroadcaster;
class MobActionSystem;

class LightningBoltSystem {
public:
    LightningBoltSystem(BlockChangeBroadcaster& broadcaster, MobActionSystem& mobs,
                        world::AdvancementTriggers* triggers = nullptr);

    using StruckCheck = std::function<bool(world::LoadedWorld&, const entity::ServerEntity&)>;

    void setStruckCheck(StruckCheck check) { struckCheck_ = std::move(check); }

    void tick(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
              const std::vector<entity::ServerEntity*>& living, entity::ServerEntity& bolt,
              random::RandomSource& random);

    static entity::ServerEntity* spawn(world::LoadedWorld& world, double x, double y, double z, std::int32_t entityId,
                                       std::optional<std::int32_t> cause = std::nullopt, bool visualOnly = false);

private:
    void hitAround(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                   const std::vector<entity::ServerEntity*>& living, entity::ServerEntity& bolt);

    void announceStrike(const world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                        const std::vector<entity::ServerEntity*>& living, const entity::ServerEntity& bolt) const;

    StruckCheck struckCheck_;
    BlockChangeBroadcaster* broadcaster_;
    MobActionSystem* mobs_;
    world::AdvancementTriggers* triggers_;
};

} // namespace basalt::server::runtime
