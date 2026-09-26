#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/entity/BlockRay.hpp>
#include <basalt/server/runtime/entity/Horses.hpp>
#include <basalt/server/runtime/entity/MobBreathing.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/LlamaGoals.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::runtime::entity::ai {
namespace {

namespace reg = server::registry;
namespace mth = wire::common::mth;

/// parity: `LlamaFollowCaravanGoal.CARAVAN_LIMIT`, how far up the line it looks for a lead.
constexpr std::int32_t kCaravanLimit = 8;
/// parity: the speed `stop` puts back, the double literal and not the float it started from.
constexpr double kCaravanRestSpeed = 2.1;
/// parity: past twenty-six blocks a follower hurries, by a fifth at a time up to three.
constexpr double kCaravanStretch = 676.0;
constexpr double kCaravanFastest = 3.0;
constexpr double kCaravanSpeedUp = 1.2;
constexpr std::int32_t kCaravanCheckTicks = 40;
/// parity: the two blocks a follower keeps back, and the two it has to be beyond to join.
constexpr double kCaravanGap = 2.0;
constexpr double kCaravanTooClose = 4.0;
/// parity: `inflate(9.0, 4.0, 9.0)`, where it looks for a line to join.
constexpr double kCaravanReach = 9.0;
constexpr double kCaravanReachUp = 4.0;
/// parity: `Vec3.normalize`'s floor, a float widened.
constexpr double kNormalizeFloor = static_cast<double>(1.0E-5F);

/// parity: `Player`'s eye standing and crouching, and `LivingEntity.hasLineOfSight`'s reach.
constexpr double kPlayerEye = 1.62;
constexpr double kPlayerCrouchingEye = 1.27;
constexpr double kSightRange = 128.0;
/// parity: `RangedAttackGoal`'s five ticks of sight before it stands still, and its look speed.
constexpr std::int32_t kSeeTime = 5;
constexpr float kLookSpeed = 30.0F;

constexpr double kDefaultFollowRange = 16.0;
/// parity: `LlamaAttackWolfGoal`: one roll in sixteen, a quarter of the follow range, four up.
constexpr std::int32_t kWolfInterval = 16;
constexpr double kWolfRangeShare = 0.25;
constexpr double kSearchUp = 4.0;
/// parity: `TargetingConditions.test`, which never shrinks the range under two blocks.
constexpr double kMinTargetRange = 2.0;

/// parity: `Wolf.registerGoals`, twenty-four blocks at one and a half, and a strength roll of five.
constexpr double kWolfAvoidRange = 24.0;
constexpr double kWolfAvoidSpeed = 1.5;
constexpr std::int32_t kStrengthRoll = 5;

[[nodiscard]] bool alive(const ServerEntity& entity) { return !entity.removed && !entity.dying && entity.health > 0.0; }

[[nodiscard]] bool leashed(const ServerEntity& entity) { return entity.leashHolderId.has_value(); }

[[nodiscard]] ServerEntity* llamaById(const MobContext& mob, std::optional<std::int32_t> id) {
    if (!id.has_value()) {
        return nullptr;
    }
    ServerEntity* found = mob.entityById(*id);
    return found != nullptr && horses::isLlama(found->type()) ? found : nullptr;
}

[[nodiscard]] double distanceSquared(const ServerEntity& from, const ServerEntity& to) {
    const double dx = from.x - to.x;
    const double dy = from.y - to.y;
    const double dz = from.z - to.z;
    return (dx * dx) + (dy * dy) + (dz * dz);
}

/// parity: `AABB.intersects` of `other`'s box against the mob's grown by `across` and `up`.
[[nodiscard]] bool withinBox(const ServerEntity& mob, const ServerEntity& other, double across, double up) {
    const double half = (mob.width() / 2.0) + across;
    const double otherHalf = other.width() / 2.0;
    return mob.x - half < other.x + otherHalf && mob.x + half > other.x - otherHalf &&
           mob.y - up < other.y + other.height() && mob.y + mob.height() + up > other.y &&
           mob.z - half < other.z + otherHalf && mob.z + half > other.z - otherHalf;
}

/// parity: `LlamaFollowCaravanGoal.firstIsLeashed`, up the line at most eight llamas.
[[nodiscard]] bool firstIsLeashed(const MobContext& mob, const ServerEntity& current, std::int32_t counter) {
    if (counter > kCaravanLimit) {
        return false;
    }
    const ServerEntity* head = llamaById(mob, current.caravanHead);
    if (head == nullptr) {
        return false;
    }
    return leashed(*head) || firstIsLeashed(mob, *head, counter + 1);
}

[[nodiscard]] double followRange(const MobContext& mob) {
    return reg::entityAttributes::baseValue(mob.mob.type(), reg::entityAttributes::kFollowRange)
        .value_or(kDefaultFollowRange);
}

/// Where something the mob is after stands, and where its eyes are.
struct Seen {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double height = 0.0;
    double eyeY = 0.0;
};

/// The entity or player by that id while it is alive and may be fought.
[[nodiscard]] std::optional<Seen> resolve(const MobContext& mob, std::int32_t id) {
    if (const PlayerView* player = mob.playerById(id); player != nullptr) {
        if (!player->targetable) {
            return std::nullopt;
        }
        return Seen{.x = player->x,
                    .y = player->y,
                    .z = player->z,
                    .height = player->height,
                    .eyeY = player->y + (player->sneaking ? kPlayerCrouchingEye : kPlayerEye)};
    }
    const ServerEntity* entity = mob.entityById(id);
    if (entity == nullptr || !alive(*entity)) {
        return std::nullopt;
    }
    return Seen{.x = entity->x,
                .y = entity->y,
                .z = entity->z,
                .height = entity->height(),
                .eyeY = entity->y + mobBreathing::eyeHeightOf(*entity)};
}

/// parity: `LivingEntity.hasLineOfSight`, eye to eye through the collision shapes.
[[nodiscard]] bool lineOfSight(MobContext& mob, const Seen& target) {
    const double eyeY = mob.mob.y + mobBreathing::eyeHeightOf(mob.mob);
    const double dx = target.x - mob.mob.x;
    const double dy = target.eyeY - eyeY;
    const double dz = target.z - mob.mob.z;
    if (std::sqrt((dx * dx) + (dy * dy) + (dz * dz)) > kSightRange) {
        return false;
    }
    return !blockRay::cast(mob.world, mob.mob.x, eyeY, mob.mob.z, target.x, target.eyeY, target.z).has_value();
}

/// parity: `TargetGoal.canContinueToUse`, without a sight check: the mob's target, else the one
/// the goal remembers, alive and inside the follow distance.
[[nodiscard]] bool keepTarget(MobContext& mob, std::optional<std::int32_t> remembered, double within) {
    const std::optional<std::int32_t> id = mob.mob.targetId.has_value() ? mob.mob.targetId : remembered;
    if (!id.has_value()) {
        return false;
    }
    const std::optional<Seen> target = resolve(mob, *id);
    if (!target.has_value() || mob.distanceSquaredTo(target->x, target->y, target->z) > within * within) {
        return false;
    }
    mob.mob.targetId = id;
    return true;
}

/// parity: `TargetGoal.canAttack` with `TargetingConditions.DEFAULT`: alive, fair game, and inside
/// the home a lead gives the mob.
[[nodiscard]] bool canAttack(const MobContext& mob, std::optional<std::int32_t> id) {
    if (!id.has_value() || *id == mob.mob.entityId()) {
        return false;
    }
    const std::optional<Seen> target = resolve(mob, *id);
    if (!target.has_value()) {
        return false;
    }
    if (!mob.mob.homePosition.has_value()) {
        return true;
    }
    const std::array<std::int32_t, 3>& home = *mob.mob.homePosition;
    const auto dx = static_cast<double>(mth::floor(target->x) - home[0]);
    const auto dy = static_cast<double>(mth::floor(target->y) - home[1]);
    const auto dz = static_cast<double>(mth::floor(target->z) - home[2]);
    return (dx * dx) + (dy * dy) + (dz * dz) < static_cast<double>(mob.mob.homeRadius * mob.mob.homeRadius);
}

} // namespace

LlamaFollowCaravanGoal::LlamaFollowCaravanGoal(double speedModifier) : speedModifier_(speedModifier) {}

bool LlamaFollowCaravanGoal::canUse(MobContext& mob) {
    ServerEntity& self = mob.mob;
    if (leashed(self) || self.caravanHead.has_value()) {
        return false;
    }
    std::vector<ServerEntity*> llamas;
    for (ServerEntity* other : mob.entities) {
        if (other != nullptr && other != &self && !other->removed && horses::isLlama(other->type()) &&
            withinBox(self, *other, kCaravanReach, kCaravanReachUp)) {
            llamas.push_back(other);
        }
    }
    ServerEntity* closest = nullptr;
    double closestDistance = std::numeric_limits<double>::max();
    // parity: the tail of a line first, and only then a llama on a lead; a tie goes to the later one.
    for (ServerEntity* candidate : llamas) {
        if (candidate->caravanHead.has_value() && !candidate->caravanTail.has_value()) {
            const double distance = distanceSquared(self, *candidate);
            if (!(distance > closestDistance)) {
                closestDistance = distance;
                closest = candidate;
            }
        }
    }
    if (closest == nullptr) {
        for (ServerEntity* candidate : llamas) {
            if (leashed(*candidate) && !candidate->caravanTail.has_value()) {
                const double distance = distanceSquared(self, *candidate);
                if (!(distance > closestDistance)) {
                    closestDistance = distance;
                    closest = candidate;
                }
            }
        }
    }
    if (closest == nullptr || closestDistance < kCaravanTooClose) {
        return false;
    }
    if (!leashed(*closest) && !firstIsLeashed(mob, *closest, 1)) {
        return false;
    }
    // parity: `Llama.joinCaravan`, which `canUse` does itself.
    self.caravanHead = closest->entityId();
    closest->caravanTail = self.entityId();
    return true;
}

bool LlamaFollowCaravanGoal::canContinueToUse(MobContext& mob) {
    const ServerEntity* head = llamaById(mob, mob.mob.caravanHead);
    if (head == nullptr || !alive(*head) || !firstIsLeashed(mob, mob.mob, 0)) {
        return false;
    }
    if (distanceSquared(mob.mob, *head) > kCaravanStretch) {
        if (speedModifier_ <= kCaravanFastest) {
            speedModifier_ *= kCaravanSpeedUp;
            distCheckCounter_ = reducedTickDelay(kCaravanCheckTicks);
            return true;
        }
        if (distCheckCounter_ == 0) {
            return false;
        }
    }
    if (distCheckCounter_ > 0) {
        --distCheckCounter_;
    }
    return true;
}

void LlamaFollowCaravanGoal::stop(MobContext& mob) {
    // parity: `Llama.leaveCaravan`.
    if (ServerEntity* head = llamaById(mob, mob.mob.caravanHead); head != nullptr) {
        head->caravanTail.reset();
    }
    mob.mob.caravanHead.reset();
    speedModifier_ = kCaravanRestSpeed;
}

void LlamaFollowCaravanGoal::tick(MobContext& mob) {
    ServerEntity& self = mob.mob;
    // parity: a llama tied to a fence stays put, caravan or not.
    if (self.leashHolderId.has_value()) {
        const ServerEntity* holder = mob.entityById(*self.leashHolderId);
        if (holder != nullptr && holder->type().path() == "leash_knot") {
            return;
        }
    }
    const ServerEntity* head = llamaById(mob, self.caravanHead);
    if (head == nullptr) {
        return;
    }
    // parity: `Entity.distanceTo`, worked out in float.
    const auto fx = static_cast<float>(self.x - head->x);
    const auto fy = static_cast<float>(self.y - head->y);
    const auto fz = static_cast<float>(self.z - head->z);
    const auto distance =
        static_cast<double>(static_cast<float>(std::sqrt(static_cast<double>((fx * fx) + (fy * fy) + (fz * fz)))));
    const double ax = head->x - self.x;
    const double ay = head->y - self.y;
    const double az = head->z - self.z;
    const double length = std::sqrt((ax * ax) + (ay * ay) + (az * az));
    const double scale = std::max(distance - kCaravanGap, 0.0);
    double dx = 0.0;
    double dy = 0.0;
    double dz = 0.0;
    if (!(length < kNormalizeFloor)) {
        dx = (ax / length) * scale;
        dy = (ay / length) * scale;
        dz = (az / length) * scale;
    }
    static_cast<void>(navigation::moveTo(mob.world, self, self.x + dx, self.y + dy, self.z + dz, speedModifier_));
}

LlamaSpitGoal::LlamaSpitGoal(double speedModifier, std::int32_t attackIntervalMin, std::int32_t attackIntervalMax,
                             float attackRadius)
    : speedModifier_(speedModifier), attackIntervalMin_(attackIntervalMin), attackIntervalMax_(attackIntervalMax),
      attackRadius_(attackRadius) {}

bool LlamaSpitGoal::canUse(MobContext& mob) {
    const std::optional<TargetView> target = targetOf(mob);
    if (!target.has_value() || (target->entity != nullptr && !alive(*target->entity))) {
        return false;
    }
    target_ = target->playerId.has_value() ? *target->playerId : target->entity->entityId();
    return true;
}

bool LlamaSpitGoal::canContinueToUse(MobContext& mob) {
    return canUse(mob) || (target_.has_value() && resolve(mob, *target_).has_value() && !navigation::isDone(mob.mob));
}

void LlamaSpitGoal::stop(MobContext& /*mob*/) {
    target_.reset();
    seeTime_ = 0;
    attackTime_ = -1;
}

void LlamaSpitGoal::tick(MobContext& mob) {
    const std::optional<Seen> target = target_.has_value() ? resolve(mob, *target_) : std::nullopt;
    if (!target.has_value()) {
        return;
    }
    const double distanceSquared = mob.distanceSquaredTo(target->x, target->y, target->z);
    const bool sight = lineOfSight(mob, *target);
    seeTime_ = sight ? seeTime_ + 1 : 0;
    if (!(distanceSquared > static_cast<double>(attackRadius_ * attackRadius_)) && seeTime_ >= kSeeTime) {
        navigation::stop(mob.mob);
    } else {
        static_cast<void>(navigation::moveTo(mob.world, mob.mob, target->x, target->y, target->z, speedModifier_));
    }
    lookControl::lookAt(mob.mob, target->x, target->eyeY, target->z, kLookSpeed, kLookSpeed);
    const auto spread = static_cast<float>(attackIntervalMax_ - attackIntervalMin_);
    if (--attackTime_ == 0) {
        if (!sight) {
            return;
        }
        // parity: `Llama.performRangedAttack` spits whatever the power, so only the clock reads it.
        const float reach = static_cast<float>(std::sqrt(distanceSquared)) / attackRadius_;
        mob.actions.llamaSpit(mob.mob, target->x, target->y, target->z, target->height);
        attackTime_ = mth::floor(static_cast<double>((reach * spread) + static_cast<float>(attackIntervalMin_)));
    } else if (attackTime_ < 0) {
        const double share = std::sqrt(distanceSquared) / static_cast<double>(attackRadius_);
        attackTime_ = mth::floor(attackIntervalMin_ + (share * (attackIntervalMax_ - attackIntervalMin_)));
    }
}

bool LlamaHurtByTargetGoal::canContinueToUse(MobContext& mob) {
    if (mob.mob.didSpit) {
        mob.mob.didSpit = false;
        return false;
    }
    return revenge_.canContinueToUse(mob);
}

bool LlamaAttackWolfGoal::canUse(MobContext& mob) {
    if (mob.random.nextInt(reducedTickDelay(kWolfInterval)) != 0) {
        return false;
    }
    const ServerEntity& self = mob.mob;
    const double range = followRange(mob) * kWolfRangeShare;
    const double limit = std::max(range, kMinTargetRange);
    const double eyeY = self.y + mobBreathing::eyeHeightOf(self);
    target_.reset();
    std::optional<double> best;
    // parity: `getNearestEntity`, measured from the llama's eyes, the first of a tie kept.
    for (const ServerEntity* wolf : mob.entities) {
        if (wolf == nullptr || wolf == &self || !alive(*wolf) || wolf->type().path() != "wolf" ||
            wolf->owner.has_value() || !withinBox(self, *wolf, range, kSearchUp) ||
            distanceSquared(self, *wolf) > limit * limit) {
            continue;
        }
        const double dx = wolf->x - self.x;
        const double dy = wolf->y - eyeY;
        const double dz = wolf->z - self.z;
        const double distance = (dx * dx) + (dy * dy) + (dz * dz);
        if (!best.has_value() || distance < *best) {
            best = distance;
            target_ = wolf->entityId();
        }
    }
    return target_.has_value();
}

bool LlamaAttackWolfGoal::canContinueToUse(MobContext& mob) {
    return keepTarget(mob, target_, followRange(mob) * kWolfRangeShare);
}

void LlamaAttackWolfGoal::start(MobContext& mob) { mob.mob.targetId = target_; }

void LlamaAttackWolfGoal::stop(MobContext& mob) {
    mob.mob.targetId.reset();
    target_.reset();
}

bool TraderLlamaDefendTraderGoal::canUse(MobContext& mob) {
    if (!mob.mob.leashHolderId.has_value()) {
        return false;
    }
    const ServerEntity* trader = mob.entityById(*mob.mob.leashHolderId);
    if (trader == nullptr || trader->type().path() != "wandering_trader") {
        return false;
    }
    ownerLastHurtBy_ = trader->lastHurtByMob;
    return trader->lastHurtByMobTimestamp != timestamp_ && canAttack(mob, ownerLastHurtBy_);
}

bool TraderLlamaDefendTraderGoal::canContinueToUse(MobContext& mob) {
    return keepTarget(mob, std::nullopt, followRange(mob));
}

void TraderLlamaDefendTraderGoal::start(MobContext& mob) {
    mob.mob.targetId = ownerLastHurtBy_;
    if (mob.mob.leashHolderId.has_value()) {
        if (const ServerEntity* trader = mob.entityById(*mob.mob.leashHolderId);
            trader != nullptr && trader->type().path() == "wandering_trader") {
            timestamp_ = trader->lastHurtByMobTimestamp;
        }
    }
}

void TraderLlamaDefendTraderGoal::stop(MobContext& mob) { mob.mob.targetId.reset(); }

WolfAvoidLlamaGoal::WolfAvoidLlamaGoal()
    : avoid_(std::set<std::string, std::less<>>{"llama", "trader_llama"}, kWolfAvoidRange, kWolfAvoidSpeed,
             kWolfAvoidSpeed) {}

bool WolfAvoidLlamaGoal::canUse(MobContext& mob) {
    if (!avoid_.canUse(mob)) {
        return false;
    }
    const ServerEntity* llama = avoid_.threat();
    // parity: `avoidLlama`, rolled only for a wolf nobody tamed.
    return llama != nullptr && !mob.mob.owner.has_value() &&
           horses::strength(*llama) >= mob.random.nextInt(kStrengthRoll);
}

void WolfAvoidLlamaGoal::start(MobContext& mob) {
    mob.mob.targetId.reset();
    avoid_.start(mob);
}

void WolfAvoidLlamaGoal::tick(MobContext& mob) {
    mob.mob.targetId.reset();
    avoid_.tick(mob);
}

} // namespace basalt::server::runtime::entity::ai
