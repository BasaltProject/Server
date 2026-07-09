#include <basalt/server/runtime/entity/MobMisc.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/FishGoals.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace basalt::server::runtime::entity::ai {
namespace {

constexpr double kSchoolReach = 8.0;
constexpr double kLeaderRangeSquared = 121.0;
constexpr std::int32_t kStartInterval = 200;

[[nodiscard]] mobMisc::Lookup lookupOf(MobContext& mob) {
    return [&mob](std::int32_t id) { return mob.entityById(id); };
}

[[nodiscard]] bool near(const ServerEntity& a, const ServerEntity& b) {
    const double reachX = kSchoolReach + ((a.width() + b.width()) / 2.0);
    const double reachY = kSchoolReach + ((a.height() + b.height()) / 2.0);
    return std::abs(a.x - b.x) <= reachX && std::abs(a.z - b.z) <= reachX && std::abs(a.y - b.y) <= reachY;
}

} // namespace

bool FollowFlockLeaderGoal::canUse(MobContext& mob) {
    ServerEntity& fish = mob.mob;
    const mobMisc::Lookup lookup = lookupOf(mob);
    // parity: `AbstractSchoolingFish.tick`, a school left alone forgets it was one.
    if (mobMisc::hasFollowers(fish) && mob.random.nextInt(kStartInterval) == 1) {
        const bool alone = std::ranges::none_of(mob.entities, [&](const ServerEntity* other) {
            return other != nullptr && other != &fish && !other->removed && other->type() == fish.type() &&
                   near(fish, *other);
        });
        if (alone) {
            mobMisc::of(fish).schoolSize = 1;
        }
    }
    if (mobMisc::hasFollowers(fish)) {
        return false;
    }
    if (mobMisc::isFollower(fish, lookup)) {
        return true;
    }
    if (nextStartTick_ < 0) {
        nextStartTick_ = reducedTickDelay(kStartInterval + (mob.random.nextInt(kStartInterval) % 20));
    }
    if (nextStartTick_ > 0) {
        --nextStartTick_;
        return false;
    }
    nextStartTick_ = reducedTickDelay(kStartInterval + (mob.random.nextInt(kStartInterval) % 20));
    std::vector<ServerEntity*> candidates;
    for (ServerEntity* other : mob.entities) {
        if (other != nullptr && !other->removed && other->type() == fish.type() && near(fish, *other) &&
            (mobMisc::canBeFollowed(*other) || !mobMisc::isFollower(*other, lookup))) {
            candidates.push_back(other);
        }
    }
    ServerEntity* leader = &fish;
    for (ServerEntity* other : candidates) {
        if (mobMisc::canBeFollowed(*other)) {
            leader = other;
            break;
        }
    }
    const std::int32_t room = mobMisc::maxSchoolSize(*leader) - mobMisc::of(*leader).schoolSize;
    std::int32_t taken = 0;
    for (ServerEntity* other : candidates) {
        if (taken >= room) {
            break;
        }
        ++taken;
        if (other != leader && !mobMisc::isFollower(*other, lookup)) {
            mobMisc::startFollowing(*other, *leader);
        }
    }
    return mobMisc::isFollower(fish, lookup);
}

bool FollowFlockLeaderGoal::canContinueToUse(MobContext& mob) {
    const mobMisc::Lookup lookup = lookupOf(mob);
    if (!mobMisc::isFollower(mob.mob, lookup)) {
        return false;
    }
    const ServerEntity* leader = mob.entityById(*mobMisc::peek(mob.mob)->schoolLeader);
    return leader != nullptr && mob.distanceSquaredTo(leader->x, leader->y, leader->z) <= kLeaderRangeSquared;
}

void FollowFlockLeaderGoal::start(MobContext& /*mob*/) { timeToRecalcPath_ = 0; }

void FollowFlockLeaderGoal::stop(MobContext& mob) { mobMisc::stopFollowing(mob.mob, lookupOf(mob)); }

void FollowFlockLeaderGoal::tick(MobContext& mob) {
    constexpr std::int32_t kRecalcInterval = 10;
    if (--timeToRecalcPath_ > 0) {
        return;
    }
    timeToRecalcPath_ = adjustedTickDelay(kRecalcInterval);
    const mobMisc::Lookup lookup = lookupOf(mob);
    if (!mobMisc::isFollower(mob.mob, lookup)) {
        return;
    }
    if (const ServerEntity* leader = mob.entityById(*mobMisc::peek(mob.mob)->schoolLeader); leader != nullptr) {
        static_cast<void>(navigation::moveTo(mob.world, mob.mob, leader->x, leader->y, leader->z, 1.0));
    }
}

} // namespace basalt::server::runtime::entity::ai
