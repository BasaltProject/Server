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

/// How entities shove each other apart when their boxes overlap.
///
/// parity: `LivingEntity.pushEntities` with `EntitySelector.pushableBy`, `Entity.push(Entity)` and
/// the vehicles' own `push`: `AbstractMinecart` and `AbstractBoat`.
class EntityPushSystem {
public:
    /// A mob or a player, whichever takes part.
    struct Body {
        entity::ServerEntity* mob = nullptr;
        player::PlayerSession* player = nullptr;

        [[nodiscard]] bool operator==(const Body&) const = default;
    };

    struct Hooks {
        entity::entityCombat::Hooks combat;
        /// parity: `Warden.doPush`, the anger and the disturbance a touch leaves.
        std::function<void(world::LoadedWorld& world, entity::ServerEntity& warden, const Body& touched)> wardenTouched;
        /// parity: `IronGolem.doPush`, which takes a monster it bumps into as its target.
        std::function<void(entity::ServerEntity& golem, entity::ServerEntity& monster)> golemTouched;
    };

    struct Options {
        std::function<std::int64_t()> nextSeed;
        const world::realBlockTags::TagMembers* blockTags = nullptr;
        const scoreboard::Teams* teams = nullptr;
        /// parity: `AbstractMinecart.useExperimentalMovement`, which changes how carts push carts.
        std::function<bool()> minecartImprovements;
    };

    EntityPushSystem(player::PlayerRegistry& players, CombatSystem& combat, Hooks hooks, Options options);

    /// parity: `LivingEntity.pushEntities` for a mob, the cramming hit included.
    void pushByMob(world::LoadedWorld& world, entity::ServerEntity& mob);

    /// parity: `LivingEntity.pushEntities` for a player.
    void pushByPlayer(world::LoadedWorld& world, player::PlayerSession& player);

    /// parity: `Entity.baseTick`'s `updateFluidInteraction`: the water and lava it stands in carry it.
    void pushByFluid(world::LoadedWorld& world, entity::ServerEntity& entity);

    /// The same on the motion the server keeps for a player.
    void pushByFluid(world::LoadedWorld& world, player::PlayerSession& player);

    /// parity: `target.push(pusher)`, dispatched on what the target is.
    void push(world::LoadedWorld& world, const Body& target, const Body& pusher);

    /// parity: `Entity.push(x, y, z)`.
    static void addMotion(const Body& body, double x, double y, double z);

    /// parity: `Entity.isPushable`, per class.
    [[nodiscard]] bool isPushable(world::LoadedWorld& world, const Body& body) const;

    /// Everything whose box overlaps `box` and that `pusher` may push, as `getPushableEntities`.
    [[nodiscard]] std::vector<Body> pushablesIn(world::LoadedWorld& world, const Body& pusher,
                                                const entity::Aabb& box) const;

    /// Everything whose box overlaps `box`, but `self`. parity: `Level.getEntities(entity, box)`.
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
