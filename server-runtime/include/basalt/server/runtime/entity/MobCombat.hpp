#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <array>
#include <cstdint>
#include <vector>

namespace basalt::server::runtime::world {
class WorldState;
}

namespace basalt::server::runtime::entity {

class ServerEntity;

/// The mobs that come for you: who they pick, how they get there, and when they swing.
///
/// parity: `NearestAttackableTargetGoal` for the picking and `MeleeAttackGoal` for the rest. A mob
/// only walks the path its navigation found, and only hits when its own box, widened by its reach,
/// touches the target's.
namespace mobCombat {

/// parity: `Mob.DEFAULT_ATTACK_REACH`, `sqrt(2.04) - 0.6`.
[[nodiscard]] double attackReach();

/// parity: `MeleeAttackGoal.attackInterval`, a swing a second.
inline constexpr std::int32_t kAttackInterval = 20;

/// parity: `NearestAttackableTargetGoal`'s own one-in-ten roll before it even looks.
inline constexpr std::int32_t kTargetInterval = 10;

/// Whether this kind of mob hunts players by walking up and hitting them.
[[nodiscard]] bool isMelee(const wire::common::Identifier& type);

/// parity: `Spider.SpiderTargetGoal`: a spider in the light leaves you alone.
[[nodiscard]] bool needsDarkness(const wire::common::Identifier& type);

/// Whoever a mob can go after: a player, reduced to what the goals actually read.
struct Target {
    std::int32_t entityId = 0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double width = 0.0;
    double height = 0.0;
};

/// The nearest target within the mob's own follow range, or nothing.
///
/// parity: `NearestAttackableTargetGoal.canUse`, roll included: it does not even look nine ticks out
/// of ten, which is what staggers a room full of zombies rather than turning them at once.
[[nodiscard]] const Target* acquire(const ServerEntity& mob, const std::vector<Target>& targets,
                                    random::RandomSource& random, std::int32_t light);

/// One tick of chasing `target`. Answers true when the mob swung, which is the caller's cue to hurt
/// whoever it swung at.
///
/// parity: `MeleeAttackGoal.tick`: look, walk the path, count the cooldown down, and hit when both
/// the cooldown and the reach allow it.
bool tick(world::WorldState& world, ServerEntity& mob, const Target& target, random::RandomSource& random,
          double speed);

/// parity: `Mob.isWithinMeleeAttackRange`: the mob's own box widened by its reach has to touch the
/// target's box, which is why a tall player is hit from further below than a short one.
[[nodiscard]] bool withinReach(const ServerEntity& mob, const Target& target);

/// Where the mob is standing, for the light check the spiders need.
[[nodiscard]] std::array<std::int32_t, 3> blockPosition(const ServerEntity& mob);

} // namespace mobCombat

} // namespace basalt::server::runtime::entity
