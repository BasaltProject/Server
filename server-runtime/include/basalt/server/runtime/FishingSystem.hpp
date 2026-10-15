#pragma once

#include <basalt/server/runtime/entity/FishingHooks.hpp>

#include <cstdint>
#include <functional>
#include <vector>

namespace basalt::server::runtime::entity {
class ServerEntity;
}

namespace basalt::server::runtime::player {
struct BreakContext;
class PlayerRegistry;
class PlayerSession;
} // namespace basalt::server::runtime::player

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime {

class ExperienceOrbSystem;

class FishingSystem {
public:
    struct Options {
        std::function<std::int64_t()> nextSeed;
    };

    FishingSystem(player::PlayerRegistry& players, ExperienceOrbSystem& orbs, Options options);

    [[nodiscard]] static bool isHook(const entity::ServerEntity& entity);

    [[nodiscard]] static bool isRod(std::string_view item);

    bool useRod(const player::BreakContext& context, player::PlayerSession& player, world::LoadedWorld& world,
                std::int32_t slot);

    void tick(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
              const std::vector<entity::ServerEntity*>& living, entity::ServerEntity& hook);

private:
    [[nodiscard]] player::PlayerSession* ownerOf(const entity::ServerEntity& hook) const;

    bool stopsFishing(world::LoadedWorld& world, entity::ServerEntity& hook, const player::PlayerSession* owner);

    void discard(entity::ServerEntity& hook);

    void cast(const player::BreakContext& context, player::PlayerSession& player, world::LoadedWorld& world,
              std::int32_t slot);

    std::int32_t retrieve(const player::BreakContext& context, player::PlayerSession& player, world::LoadedWorld& world,
                          entity::ServerEntity& hook, std::int32_t slot);

    void bob(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers, entity::ServerEntity& hook,
             entity::fishingHooks::HookState& state, float liquidHeight, bool inWater);

    void hookFirstHit(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                      const std::vector<entity::ServerEntity*>& living, entity::ServerEntity& hook,
                      entity::fishingHooks::HookState& state);

    player::PlayerRegistry* players_;
    ExperienceOrbSystem* orbs_;
    Options options_;
};

} // namespace basalt::server::runtime
