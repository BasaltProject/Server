#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/entity/Breeding.hpp>
#include <basalt/server/runtime/entity/LivingMovement.hpp>
#include <basalt/server/runtime/entity/MobBrain.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/Goals.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <utility>

namespace basalt::server::runtime::entity::ai {
namespace {

namespace reg = server::registry;

constexpr double kPlayerEye = 1.62;
constexpr double kEyeShare = 0.85;

[[nodiscard]] std::int32_t blockOf(double coordinate) { return static_cast<std::int32_t>(std::floor(coordinate)); }

[[nodiscard]] std::string_view blockPathAt(world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> state = world.blockState(x, y, z);
    if (!state.has_value()) {
        return {};
    }
    const wire::common::Identifier* block = reg::blockStateDefinitions::blockOf(*state);
    return block == nullptr ? std::string_view{} : std::string_view{block->path()};
}

} // namespace

std::optional<TargetView> targetOf(const MobContext& mob) {
    if (!mob.mob.targetId.has_value()) {
        return std::nullopt;
    }
    const std::int32_t id = *mob.mob.targetId;
    if (const PlayerView* player = mob.playerById(id); player != nullptr) {
        if (!player->targetable) {
            return std::nullopt;
        }
        return TargetView{.playerId = player->entityId,
                          .x = player->x,
                          .y = player->y,
                          .z = player->z,
                          .width = player->width,
                          .height = player->height};
    }
    ServerEntity* entity = mob.entityById(id);
    if (entity == nullptr || entity->removed) {
        return std::nullopt;
    }
    return TargetView{.entity = entity,
                      .x = entity->x,
                      .y = entity->y,
                      .z = entity->z,
                      .width = entity->width(),
                      .height = entity->height()};
}

bool FloatGoal::canUse(MobContext& mob) {
    const std::string_view path = blockPathAt(mob.world, blockOf(mob.mob.x), blockOf(mob.mob.y), blockOf(mob.mob.z));
    return path == "water" || path == "lava";
}

void FloatGoal::tick(MobContext& mob) {
    // parity: `FloatGoal.tick`, four times out of five.
    if (mob.random.nextFloat() < 0.8F) {
        livingMovement::jump(mob.world, mob.mob);
    }
}

RandomStrollGoal::RandomStrollGoal(double speedModifier, std::int32_t interval)
    : speedModifier_(speedModifier), interval_(interval) {}

bool RandomStrollGoal::canUse(MobContext& mob) {
    // parity: a mob nobody has been near for five seconds stops wandering.
    if (mob.mob.idleTicks >= 100) {
        return false;
    }
    if (mob.random.nextInt(reducedTickDelay(interval_)) != 0) {
        return false;
    }
    wanted_ = mobBrain::strollTarget(mob.world, mob.mob, mob.random);
    return wanted_.has_value();
}

bool RandomStrollGoal::canContinueToUse(MobContext& mob) { return !navigation::isDone(mob.mob); }

void RandomStrollGoal::start(MobContext& mob) {
    if (wanted_.has_value()) {
        static_cast<void>(
            navigation::moveTo(mob.world, mob.mob, (*wanted_)[0], (*wanted_)[1], (*wanted_)[2], speedModifier_));
    }
}

void RandomStrollGoal::stop(MobContext& mob) { navigation::stop(mob.mob); }

LookAtPlayerGoal::LookAtPlayerGoal(double lookDistance, float probability)
    : lookDistance_(lookDistance), probability_(probability) {}

bool LookAtPlayerGoal::canUse(MobContext& mob) {
    if (mob.random.nextFloat() >= probability_) {
        return false;
    }
    const PlayerView* player = mob.nearestPlayer(lookDistance_);
    lookAt_ = player == nullptr ? std::nullopt : std::optional{player->entityId};
    return lookAt_.has_value();
}

bool LookAtPlayerGoal::canContinueToUse(MobContext& mob) {
    const PlayerView* player = lookAt_.has_value() ? mob.playerById(*lookAt_) : nullptr;
    if (player == nullptr || mob.distanceSquaredTo(player->x, player->y, player->z) > lookDistance_ * lookDistance_) {
        return false;
    }
    return lookTime_ > 0;
}

void LookAtPlayerGoal::start(MobContext& mob) { lookTime_ = adjustedTickDelay(40 + mob.random.nextInt(40)); }

void LookAtPlayerGoal::stop(MobContext& /*mob*/) { lookAt_.reset(); }

void LookAtPlayerGoal::tick(MobContext& mob) {
    const PlayerView* player = lookAt_.has_value() ? mob.playerById(*lookAt_) : nullptr;
    if (player == nullptr) {
        return;
    }
    lookControl::lookAt(mob.mob, player->x, player->y + kPlayerEye, player->z);
    lookTime_--;
}

bool RandomLookAroundGoal::canUse(MobContext& mob) { return mob.random.nextFloat() < 0.02F; }

bool RandomLookAroundGoal::canContinueToUse(MobContext& /*mob*/) { return lookTime_ >= 0; }

void RandomLookAroundGoal::start(MobContext& mob) {
    const double angle = 2.0 * std::numbers::pi * mob.random.nextDouble();
    relX_ = std::cos(angle);
    relZ_ = std::sin(angle);
    lookTime_ = 20 + mob.random.nextInt(20);
}

void RandomLookAroundGoal::tick(MobContext& mob) {
    lookTime_--;
    lookControl::lookAt(mob.mob, mob.mob.x + relX_, mob.mob.y + mob.mob.height() * kEyeShare, mob.mob.z + relZ_);
}

PanicGoal::PanicGoal(double speedModifier) : speedModifier_(speedModifier) {}

bool PanicGoal::canUse(MobContext& mob) {
    if (mob.mob.panicTicks <= 0) {
        return false;
    }
    // parity: `PanicGoal.findRandomPosition`, five across and four up.
    wanted_ = mobBrain::strollTarget(mob.world, mob.mob, mob.random, 5, 4);
    return wanted_.has_value();
}

bool PanicGoal::canContinueToUse(MobContext& mob) { return !navigation::isDone(mob.mob); }

void PanicGoal::start(MobContext& mob) {
    if (wanted_.has_value()) {
        static_cast<void>(
            navigation::moveTo(mob.world, mob.mob, (*wanted_)[0], (*wanted_)[1], (*wanted_)[2], speedModifier_));
    }
}

void PanicGoal::stop(MobContext& mob) { navigation::stop(mob.mob); }

TemptGoal::TemptGoal(double speedModifier, std::set<std::string, std::less<>> items, double stopDistance)
    : speedModifier_(speedModifier), items_(std::move(items)), stopDistance_(stopDistance) {}

bool TemptGoal::canUse(MobContext& mob) {
    // parity: `Attributes.TEMPT_RANGE`, ten blocks for everything that has it.
    constexpr double kTemptRange = 10.0;
    player_.reset();
    double bestDistance = kTemptRange * kTemptRange;
    for (const PlayerView& player : mob.players) {
        if (!player.targetable || !player.heldItem.has_value() || !items_.contains(player.heldItem->path())) {
            continue;
        }
        const double distance = mob.distanceSquaredTo(player.x, player.y, player.z);
        if (distance > bestDistance) {
            continue;
        }
        bestDistance = distance;
        player_ = player.entityId;
    }
    return player_.has_value();
}

bool TemptGoal::canContinueToUse(MobContext& mob) {
    constexpr double kTemptRange = 10.0;
    const PlayerView* holder = player_.has_value() ? mob.playerById(*player_) : nullptr;
    if (holder == nullptr || !holder->heldItem.has_value() || !items_.contains(holder->heldItem->path())) {
        return false;
    }
    return mob.distanceSquaredTo(holder->x, holder->y, holder->z) <= kTemptRange * kTemptRange;
}

void TemptGoal::stop(MobContext& mob) {
    player_.reset();
    navigation::stop(mob.mob);
}

void TemptGoal::tick(MobContext& mob) {
    const PlayerView* holder = player_.has_value() ? mob.playerById(*player_) : nullptr;
    if (holder == nullptr) {
        return;
    }
    lookControl::lookAt(mob.mob, holder->x, holder->y + kPlayerEye, holder->z, 40.0F);
    if (mob.distanceSquaredTo(holder->x, holder->y, holder->z) < stopDistance_ * stopDistance_) {
        navigation::stop(mob.mob);
    } else {
        static_cast<void>(navigation::moveTo(mob.world, mob.mob, holder->x, holder->y, holder->z, speedModifier_));
    }
}

BreedGoal::BreedGoal(double speedModifier) : speedModifier_(speedModifier) {}

bool BreedGoal::canUse(MobContext& mob) {
    if (mob.mob.loveTicks <= 0) {
        return false;
    }
    partner_ = nullptr;
    double bestDistance = 0.0;
    for (ServerEntity* candidate : mob.entities) {
        if (candidate == nullptr || !breeding::canMate(mob.mob, *candidate)) {
            continue;
        }
        const double distance = mob.distanceSquaredTo(candidate->x, candidate->y, candidate->z);
        if (partner_ == nullptr || distance < bestDistance) {
            partner_ = candidate;
            bestDistance = distance;
        }
    }
    return partner_ != nullptr;
}

bool BreedGoal::canContinueToUse(MobContext& mob) {
    return partner_ != nullptr && !partner_->removed && mob.mob.loveTicks > 0 && partner_->loveTicks > 0;
}

void BreedGoal::start(MobContext& /*mob*/) { loveTime_ = 0; }

void BreedGoal::stop(MobContext& mob) {
    partner_ = nullptr;
    loveTime_ = 0;
    navigation::stop(mob.mob);
}

void BreedGoal::tick(MobContext& mob) {
    if (partner_ == nullptr) {
        return;
    }
    lookControl::lookAt(mob.mob, partner_->x, partner_->y + partner_->height() * kEyeShare, partner_->z, 10.0F);
    static_cast<void>(navigation::moveTo(mob.world, mob.mob, partner_->x, partner_->y, partner_->z, speedModifier_));
    loveTime_++;
}

FollowParentGoal::FollowParentGoal(double speedModifier) : speedModifier_(speedModifier) {}

bool FollowParentGoal::canUse(MobContext& mob) {
    constexpr double kFollowRange = 8.0;
    if (mob.mob.age >= 0) {
        return false;
    }
    parent_ = nullptr;
    double bestDistance = kFollowRange * kFollowRange;
    for (ServerEntity* candidate : mob.entities) {
        if (candidate == nullptr || candidate == &mob.mob || candidate->type() != mob.mob.type() ||
            candidate->age < 0 || candidate->removed) {
            continue;
        }
        const double distance = mob.distanceSquaredTo(candidate->x, candidate->y, candidate->z);
        if (distance > bestDistance) {
            continue;
        }
        bestDistance = distance;
        parent_ = candidate;
    }
    return parent_ != nullptr;
}

bool FollowParentGoal::canContinueToUse(MobContext& mob) {
    constexpr double kFollowRange = 8.0;
    constexpr double kStopRange = 3.0;
    if (parent_ == nullptr || mob.mob.age >= 0 || parent_->removed) {
        return false;
    }
    const double distance = mob.distanceSquaredTo(parent_->x, parent_->y, parent_->z);
    return distance >= kStopRange * kStopRange && distance <= kFollowRange * kFollowRange;
}

void FollowParentGoal::start(MobContext& /*mob*/) { repathCooldown_ = 0; }

void FollowParentGoal::stop(MobContext& mob) {
    parent_ = nullptr;
    navigation::stop(mob.mob);
}

void FollowParentGoal::tick(MobContext& mob) {
    if (parent_ == nullptr || --repathCooldown_ > 0) {
        return;
    }
    repathCooldown_ = adjustedTickDelay(10);
    static_cast<void>(navigation::moveTo(mob.world, mob.mob, parent_->x, parent_->y, parent_->z, speedModifier_));
}

bool EatBlockGoal::canUse(MobContext& mob) {
    // parity: `EatBlockGoal`, one roll in a thousand for a grown sheep and fifty for a lamb.
    if (mob.random.nextInt(mob.mob.age < 0 ? 50 : 1000) != 0) {
        return false;
    }
    const std::int32_t x = blockOf(mob.mob.x);
    const std::int32_t y = blockOf(mob.mob.y);
    const std::int32_t z = blockOf(mob.mob.z);
    return blockPathAt(mob.world, x, y - 1, z) == "grass_block" || blockPathAt(mob.world, x, y, z) == "short_grass";
}

bool EatBlockGoal::canContinueToUse(MobContext& /*mob*/) { return eatTicks_ > 0; }

void EatBlockGoal::start(MobContext& mob) {
    eatTicks_ = 40;
    navigation::stop(mob.mob);
}

void EatBlockGoal::stop(MobContext& /*mob*/) { eatTicks_ = 0; }

void EatBlockGoal::tick(MobContext& mob) {
    eatTicks_ = std::max(0, eatTicks_ - 1);
    if (eatTicks_ != 4) {
        return;
    }
    mob.actions.eatGrass(mob.mob);
}

AvoidEntityGoal::AvoidEntityGoal(std::set<std::string, std::less<>> avoided, double distance, double walkSpeed,
                                 double sprintSpeed)
    : avoided_(std::move(avoided)), distance_(distance), walkSpeed_(walkSpeed), sprintSpeed_(sprintSpeed) {}

bool AvoidEntityGoal::canUse(MobContext& mob) {
    threat_ = nullptr;
    double bestDistance = distance_ * distance_;
    for (ServerEntity* candidate : mob.entities) {
        if (candidate == nullptr || candidate->removed || !avoided_.contains(candidate->type().path())) {
            continue;
        }
        const double distance = mob.distanceSquaredTo(candidate->x, candidate->y, candidate->z);
        if (distance > bestDistance) {
            continue;
        }
        bestDistance = distance;
        threat_ = candidate;
    }
    if (threat_ == nullptr) {
        return false;
    }
    // parity: `DefaultRandomPos.getPosAway`, a spot roughly away from what it is avoiding.
    const std::optional<std::array<double, 3>> away = mobBrain::strollTarget(mob.world, mob.mob, mob.random);
    if (!away.has_value()) {
        return false;
    }
    const double towardsX = mob.mob.x - threat_->x;
    const double towardsZ = mob.mob.z - threat_->z;
    if (((*away)[0] - mob.mob.x) * towardsX + ((*away)[2] - mob.mob.z) * towardsZ < 0.0) {
        return false;
    }
    wanted_ = away;
    return true;
}

bool AvoidEntityGoal::canContinueToUse(MobContext& mob) { return !navigation::isDone(mob.mob); }

void AvoidEntityGoal::start(MobContext& mob) {
    if (wanted_.has_value()) {
        static_cast<void>(
            navigation::moveTo(mob.world, mob.mob, (*wanted_)[0], (*wanted_)[1], (*wanted_)[2], walkSpeed_));
    }
}

void AvoidEntityGoal::tick(MobContext& mob) {
    constexpr double kSprintRange = 7.0;
    if (threat_ == nullptr) {
        return;
    }
    // parity: it breaks into a run once whatever it is avoiding is close.
    mob.mob.moveSpeed = mob.distanceSquaredTo(threat_->x, threat_->y, threat_->z) < kSprintRange * kSprintRange
                            ? sprintSpeed_
                            : walkSpeed_;
}

void AvoidEntityGoal::stop(MobContext& mob) {
    threat_ = nullptr;
    navigation::stop(mob.mob);
}

LeapAtTargetGoal::LeapAtTargetGoal(double height) : height_(height) {}

bool LeapAtTargetGoal::canUse(MobContext& mob) {
    constexpr double kMinRange = 4.0;
    constexpr double kMaxRange = 16.0;
    if (!mob.mob.onGround) {
        return false;
    }
    const std::optional<TargetView> target = targetOf(mob);
    if (!target.has_value()) {
        return false;
    }
    const double distance = mob.distanceSquaredTo(target->x, target->y, target->z);
    if (distance < kMinRange || distance > kMaxRange) {
        return false;
    }
    return mob.random.nextInt(reducedTickDelay(5)) == 0;
}

bool LeapAtTargetGoal::canContinueToUse(MobContext& mob) { return !mob.mob.onGround; }

void LeapAtTargetGoal::start(MobContext& mob) {
    constexpr double kLeapSpeed = 0.4;
    constexpr double kKeptSpeed = 0.2;
    const std::optional<TargetView> target = targetOf(mob);
    if (!target.has_value()) {
        return;
    }
    double dx = target->x - mob.mob.x;
    double dz = target->z - mob.mob.z;
    const double length = std::sqrt(dx * dx + dz * dz);
    if (length < 1.0e-4) {
        return;
    }
    dx = dx / length * kLeapSpeed + mob.mob.velocityX * kKeptSpeed;
    dz = dz / length * kLeapSpeed + mob.mob.velocityZ * kKeptSpeed;
    mob.mob.velocityX = dx;
    mob.mob.velocityZ = dz;
    mob.mob.velocityY = height_;
    mob.mob.onGround = false;
}

bool SwellGoal::canUse(MobContext& mob) {
    constexpr double kSwellRange = 9.0;
    if (mob.mob.swell > 0) {
        return true;
    }
    const std::optional<TargetView> target = targetOf(mob);
    return target.has_value() && mob.distanceSquaredTo(target->x, target->y, target->z) < kSwellRange;
}

void SwellGoal::start(MobContext& mob) { navigation::stop(mob.mob); }

void SwellGoal::tick(MobContext& mob) {
    // parity: `Creeper`'s own three blocks to start, seven to stop, and thirty ticks of fuse.
    constexpr double kStopRange = 49.0;
    constexpr std::int32_t kMaxSwell = 30;
    const std::optional<TargetView> target = targetOf(mob);
    const std::int32_t direction =
        !target.has_value() || mob.distanceSquaredTo(target->x, target->y, target->z) > kStopRange ? -1 : 1;
    if (direction > 0 && mob.mob.swell == 0) {
        mob.actions.primeCreeper(mob.mob);
    }
    mob.mob.swell = std::max(0, mob.mob.swell + direction);
    if (mob.mob.swell < kMaxSwell) {
        return;
    }
    mob.mob.swell = kMaxSwell;
    mob.actions.explodeCreeper(mob.mob);
}

RangedAttackGoal::RangedAttackGoal(double speedModifier, std::int32_t attackIntervalMin, std::int32_t attackIntervalMax,
                                   double attackRadius)
    : speedModifier_(speedModifier), attackIntervalMin_(attackIntervalMin), attackIntervalMax_(attackIntervalMax),
      attackRadius_(attackRadius) {}

bool RangedAttackGoal::canUse(MobContext& mob) { return targetOf(mob).has_value(); }

bool RangedAttackGoal::canContinueToUse(MobContext& mob) {
    return targetOf(mob).has_value() || !navigation::isDone(mob.mob);
}

void RangedAttackGoal::stop(MobContext& mob) {
    seeTime_ = 0;
    attackTime_ = -1;
    navigation::stop(mob.mob);
}

void RangedAttackGoal::tick(MobContext& mob) {
    constexpr std::int32_t kSeeTime = 5;
    constexpr double kMinPower = 0.1;
    const std::optional<TargetView> target = targetOf(mob);
    if (!target.has_value()) {
        return;
    }
    const double distanceSquared = mob.distanceSquaredTo(target->x, target->y, target->z);
    seeTime_++;
    if (distanceSquared <= attackRadius_ * attackRadius_ && seeTime_ >= kSeeTime) {
        navigation::stop(mob.mob);
    } else {
        static_cast<void>(navigation::moveTo(mob.world, mob.mob, target->x, target->y, target->z, speedModifier_));
    }
    lookControl::lookAt(mob.mob, target->x, target->y + target->height * kEyeShare, target->z, 30.0F);
    attackTime_--;
    if (attackTime_ == 0) {
        const double reach = std::clamp(std::sqrt(distanceSquared) / attackRadius_, kMinPower, 1.0);
        mob.actions.shootArrow(mob.mob, target->x, target->y, target->z, static_cast<float>(reach));
        attackTime_ = static_cast<std::int32_t>(reach * (attackIntervalMax_ - attackIntervalMin_) + attackIntervalMin_);
    } else if (attackTime_ < 0) {
        const double reach = std::clamp(std::sqrt(distanceSquared) / attackRadius_, 0.0, 1.0);
        attackTime_ = static_cast<std::int32_t>(attackIntervalMin_ + reach * (attackIntervalMax_ - attackIntervalMin_));
    }
}

SwimAboutGoal::SwimAboutGoal(double push) : push_(push) {}

void SwimAboutGoal::tick(MobContext& mob) {
    // parity: the tenth of a block up or down a squid picks, and the ten blocks ahead it looks.
    constexpr double kRise = 0.1;
    constexpr std::int32_t kChangeInterval = 50;
    constexpr std::int32_t kIdleLimit = 100;
    constexpr double kLookAhead = 10.0;
    if (mob.mob.idleTicks > kIdleLimit) {
        vectorX_ = 0.0;
        vectorY_ = 0.0;
        vectorZ_ = 0.0;
    } else if (mob.random.nextInt(reducedTickDelay(kChangeInterval)) == 0 || (vectorX_ == 0.0 && vectorZ_ == 0.0)) {
        const auto angle = mob.random.nextFloat() * 2.0F * static_cast<float>(std::numbers::pi);
        vectorX_ = static_cast<double>(std::cos(angle)) * push_;
        vectorY_ = -kRise + static_cast<double>(mob.random.nextFloat()) * (2 * kRise);
        vectorZ_ = static_cast<double>(std::sin(angle)) * push_;
    }
    mob.mob.velocityX = vectorX_;
    mob.mob.velocityY = vectorY_;
    mob.mob.velocityZ = vectorZ_;
    // parity: `Squid.aiStep`'s own turn towards where it is going.
    lookControl::lookAt(mob.mob, mob.mob.x + vectorX_ * kLookAhead, mob.mob.y + vectorY_,
                        mob.mob.z + vectorZ_ * kLookAhead);
}

void HopAboutGoal::tick(MobContext& mob) {
    constexpr std::int32_t kTurnInterval = 40;
    constexpr float kTurnRate = 90.0F;
    constexpr float kTurnSpeed = 10.0F;
    constexpr std::int32_t kJumpDelayMin = 10;
    constexpr std::int32_t kJumpDelaySpread = 20;
    constexpr std::int32_t kAggressiveShare = 3;
    /// parity: `setWantedMovement(1.0)`, which is the whole of its speed and nothing more.
    constexpr double kForwardShare = 1.0;

    const std::optional<TargetView> target = targetOf(mob);
    if (target.has_value()) {
        lookControl::lookAt(mob.mob, target->x, target->y, target->z, kTurnSpeed);
        heading_ = mob.mob.headYaw;
    } else if (--nextTurn_ <= 0) {
        // parity: `CubeMobRandomDirectionGoal`, a new heading every few seconds.
        nextTurn_ = kTurnInterval + mob.random.nextInt(kTurnInterval);
        heading_ = mob.random.nextFloat() * 360.0F;
    }
    mob.mob.yaw = moveControl::rotlerp(mob.mob.yaw, heading_, kTurnRate);
    mob.mob.headYaw = mob.mob.yaw;
    // parity: `CubeMobMoveControl.tick`.
    if (!mob.mob.onGround) {
        moveControl::setForward(mob.mob, kForwardShare);
        return;
    }
    if (--jumpDelay_ > 0) {
        return;
    }
    jumpDelay_ = kJumpDelayMin + mob.random.nextInt(kJumpDelaySpread);
    if (target.has_value()) {
        jumpDelay_ /= kAggressiveShare;
    }
    livingMovement::jump(mob.world, mob.mob);
    moveControl::setForward(mob.mob, kForwardShare);
}

} // namespace basalt::server::runtime::entity::ai
