#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/entity/MobBrain.hpp>
#include <basalt/server/runtime/entity/MobCombat.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>

#include <cmath>
#include <set>
#include <string>

namespace basalt::server::runtime::entity::mobCombat {
namespace {

namespace reg = server::registry;

/// parity: `MeleeAttackGoal.tick`, which asks its navigation again every 4 to 10 ticks.
constexpr std::int32_t kRepathMin = 4;
constexpr std::int32_t kRepathSpread = 7;

/// parity: `Spider.SpiderTargetGoal`, which needs the light below a half to notice anybody.
constexpr std::int32_t kSpiderLight = 8;

/// The mobs this build sends after a player.
///
/// parity: the classes whose target selector holds a `NearestAttackableTargetGoal` on the player and
/// whose goal selector holds a melee attack. The ranged ones (skeletons, blazes, ghasts), the ones
/// that only fight back when hit (piglins, wolves, endermen) and the creeper, which does not swing at
/// all, are deliberately not here.
[[nodiscard]] const std::set<std::string, std::less<>>& melee() {
    static const std::set<std::string, std::less<>> all{
        "zombie",          "husk",         "drowned",   "zombie_villager", "giant",   "spider",
        "cave_spider",     "silverfish",   "endermite", "vindicator",      "ravager", "vex",
        "wither_skeleton", "piglin_brute", "hoglin",    "zoglin",          "warden"};
    return all;
}

} // namespace

double attackReach() {
    static const double reach = std::sqrt(2.04) - 0.6;
    return reach;
}

bool isMelee(const wire::common::Identifier& type) { return melee().contains(type.path()); }

bool needsDarkness(const wire::common::Identifier& type) {
    return type.path() == "spider" || type.path() == "cave_spider";
}

const Target* acquire(const ServerEntity& mob, const std::vector<Target>& targets, random::RandomSource& random,
                      std::int32_t light) {
    if (random.nextInt(kTargetInterval) != 0) {
        return nullptr;
    }
    if (needsDarkness(mob.type()) && light >= kSpiderLight) {
        return nullptr;
    }
    if (!reg::entityAttributes::baseValue(mob.type(), reg::entityAttributes::kFollowRange).has_value()) {
        return nullptr;
    }
    const double range = mob.attributeWithModifiers("minecraft:follow_range");
    const Target* best = nullptr;
    double bestDistance = range * range;
    for (const Target& target : targets) {
        const double dx = target.x - mob.x;
        const double dy = target.y - mob.y;
        const double dz = target.z - mob.z;
        const double distance = dx * dx + dy * dy + dz * dz;
        if (distance > bestDistance) {
            continue;
        }
        bestDistance = distance;
        best = &target;
    }
    return best;
}

bool withinReach(const ServerEntity& mob, const Target& target) {
    const double reach = reg::entityTypeShapes::widthOf(mob.type()) / 2.0 + attackReach() + target.width / 2.0;
    if (std::abs(target.x - mob.x) > reach || std::abs(target.z - mob.z) > reach) {
        return false;
    }
    const double mobTop = mob.y + reg::entityTypeShapes::heightOf(mob.type());
    return target.y <= mobTop && target.y + target.height >= mob.y;
}

bool tick(world::WorldState& world, ServerEntity& mob, const Target& target, random::RandomSource& random,
          double speed) {
    mob.repathCooldown--;
    if (mob.repathCooldown <= 0) {
        mob.repathCooldown = kRepathMin + random.nextInt(kRepathSpread);
        static_cast<void>(mobBrain::pathTo(world, mob, target.x, target.y, target.z));
    }
    static_cast<void>(mobBrain::walk(world, mob, speed));
    if (mob.attackCooldown > 0 || !withinReach(mob, target)) {
        return false;
    }
    mob.attackCooldown = kAttackInterval;
    return true;
}

std::array<std::int32_t, 3> blockPosition(const ServerEntity& mob) {
    return {static_cast<std::int32_t>(std::floor(mob.x)), static_cast<std::int32_t>(std::floor(mob.y)),
            static_cast<std::int32_t>(std::floor(mob.z))};
}

} // namespace basalt::server::runtime::entity::mobCombat
