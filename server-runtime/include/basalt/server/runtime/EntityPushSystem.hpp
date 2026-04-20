#pragma once

#include <basalt/server/runtime/entity/Aabb.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/world/RealBlockTags.hpp>

#include <cstdint>
#include <functional>
#include <vector>

namespace basalt::server::random {
class RandomSource;
}

namespace basalt::server::runtime::player {
class PlayerRegistry;
class PlayerSession;
} // namespace basalt::server::runtime::player

namespace basalt::server::runtime::scoreboard {
class Teams;
}

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime {

class CombatSystem;

class EntityPushSystem {
public:
    struct Body {
        entity::ServerEntity* mob = nullptr;
        player::PlayerSession* player = nullptr;

        [[nodiscard]] bool operator==(const Body&) const = default;
    };

    struct Hooks {
        entity::entityCombat::Hooks combat;
        std::function<void(world::LoadedWorld& world, entity::ServerEntity& warden, const Body& touched)> wardenTouched;
        std::function<void(entity::ServerEntity& golem, entity::ServerEntity& monster)> golemTouched;
    };

    struct Options {
        std::function<std::int64_t()> nextSeed;
        const world::realBlockTags::TagMembers* blockTags = nullptr;
        const scoreboard::Teams* teams = nullptr;
        std::function<bool()> minecartImprovements;
    };

    EntityPushSystem(player::PlayerRegistry& players, CombatSystem& combat, Hooks hooks, Options options);

    void pushByMob(world::LoadedWorld& world, entity::ServerEntity& mob);

    void pushByPlayer(world::LoadedWorld& world, player::PlayerSession& player);

    void pushByFluid(world::LoadedWorld& world, entity::ServerEntity& entity);

    void pushByFluid(world::LoadedWorld& world, player::PlayerSession& player);

    void push(world::LoadedWorld& world, const Body& target, const Body& pusher);

    static void addMotion(const Body& body, double x, double y, double z);

    [[nodiscard]] bool isPushable(world::LoadedWorld& world, const Body& body) const;

    [[nodiscard]] std::vector<Body> pushablesIn(world::LoadedWorld& world, const Body& pusher,
                                                const entity::Aabb& box) const;

    [[nodiscard]] std::vector<Body> entitiesIn(world::LoadedWorld& world, const Body& self,
                                               const entity::Aabb& box) const;

    [[nodiscard]] static entity::Aabb boxOf(const Body& body);

private:
    void doPush(world::LoadedWorld& world, const Body& pusher, const Body& target);

    void entityPush(world::LoadedWorld& world, const Body& target, const Body& pusher);

    void cartPush(world::LoadedWorld& world, entity::ServerEntity& cart, const Body& pusher);

    void crammed(world::LoadedWorld& world, const Body& body);

    [[nodiscard]] bool onClimbable(world::LoadedWorld& world, double x, double y, double z) const;

    [[nodiscard]] bool pushableBy(world::LoadedWorld& world, const Body& pusher, const Body& input) const;

    player::PlayerRegistry* players_;
    CombatSystem* combat_;
    Hooks hooks_;
    Options options_;
};

} // namespace basalt::server::runtime
