#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/Goals.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/entity/ai/PhantomGoals.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <optional>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::entity::ai {
namespace {

using Block = std::array<std::int32_t, 3>;

namespace mth = wire::common::mth;

constexpr float kRadians = std::numbers::pi_v<float> / 180.0F;

[[nodiscard]] Block blockOf(const ServerEntity& mob) {
    return {mth::floor(mob.x), mth::floor(mob.y), mth::floor(mob.z)};
}

/// parity: `Phantom.anchorPoint`, read once from what the spawn wrote as `anchor_pos`.
[[nodiscard]] std::optional<Block>& anchorOf(ServerEntity& mob) {
    std::optional<Block>& anchor = mob.flight.anchorPoint;
    if (!anchor.has_value() && mob.sourceNbt.has_value()) {
        const std::span<const std::int32_t> saved = mob.sourceNbt->getIntArray("anchor_pos");
        if (saved.size() == 3) {
            anchor = Block{saved[0], saved[1], saved[2]};
        }
    }
    return anchor;
}

void setAnchor(ServerEntity& mob, const Block& at) {
    mob.flight.anchorPoint = at;
    if (!mob.sourceNbt.has_value()) {
        mob.sourceNbt.emplace();
    }
    mob.sourceNbt->putIntArray("anchor_pos", {at[0], at[1], at[2]});
}

[[nodiscard]] bool isAir(const MobContext& mob, const Block& at) {
    const std::optional<std::int32_t> state = mob.world.blockState(at[0], at[1], at[2]);
    const wire::common::Identifier* block =
        state.has_value() ? registry::blockStateDefinitions::blockOf(*state) : nullptr;
    return block != nullptr && (block->path() == "air" || block->path() == "cave_air" || block->path() == "void_air");
}

/// parity: `canAttack(target, TargetingConditions.DEFAULT)`, which is a live, targetable target.
[[nodiscard]] bool canAttack(const MobContext& mob) { return targetOf(mob).has_value(); }

[[nodiscard]] bool touchingTarget(const ServerEntity& mob) {
    const double dx = mob.flight.moveTargetPoint[0] - mob.x;
    const double dy = mob.flight.moveTargetPoint[1] - mob.y;
    const double dz = mob.flight.moveTargetPoint[2] - mob.z;
    return (dx * dx) + (dy * dy) + (dz * dz) < 4.0;
}

void setAnchorAboveTarget(MobContext& mob) {
    const std::optional<TargetView> target = targetOf(mob);
    if (!anchorOf(mob.mob).has_value() || !target.has_value()) {
        return;
    }
    Block at{mth::floor(target->x), mth::floor(target->y) + 20 + mob.random.nextInt(20), mth::floor(target->z)};
    if (at[1] < mob.seaLevel) {
        at[1] = mob.seaLevel + 1;
    }
    setAnchor(mob.mob, at);
}

} // namespace

bool PhantomStrategyGoal::canUse(MobContext& mob) { return canAttack(mob); }

void PhantomStrategyGoal::start(MobContext& mob) {
    nextSweepTick_ = adjustedTickDelay(10);
    mob.mob.flight.swooping = false;
    setAnchorAboveTarget(mob);
}

void PhantomStrategyGoal::stop(MobContext& mob) {
    const std::optional<Block>& anchor = anchorOf(mob.mob);
    if (!anchor.has_value()) {
        return;
    }
    // parity: back up to ten to thirty blocks over the ground under the anchor.
    std::int32_t top = (*anchor)[1];
    if (const server::world::Chunk* chunk = mob.world.chunkIfLoaded((*anchor)[0] >> 4, (*anchor)[2] >> 4);
        chunk != nullptr) {
        top = chunk->heightAt((*anchor)[0] & 15, (*anchor)[2] & 15, server::world::HeightmapType::MotionBlocking);
    }
    setAnchor(mob.mob, Block{(*anchor)[0], top + 10 + mob.random.nextInt(20), (*anchor)[2]});
}

void PhantomStrategyGoal::tick(MobContext& mob) {
    if (mob.mob.flight.swooping) {
        return;
    }
    --nextSweepTick_;
    if (nextSweepTick_ <= 0) {
        mob.mob.flight.swooping = true;
        setAnchorAboveTarget(mob);
        nextSweepTick_ = adjustedTickDelay((8 + mob.random.nextInt(4)) * 20);
        static_cast<void>(mob.random.nextFloat());
        mob.actions.mobSound(mob.mob, "entity.phantom.swoop");
    }
}

bool PhantomSweepGoal::canUse(MobContext& mob) { return mob.mob.targetId.has_value() && mob.mob.flight.swooping; }

bool PhantomSweepGoal::canContinueToUse(MobContext& mob) {
    if (!targetOf(mob).has_value() || !canUse(mob)) {
        return false;
    }
    if (mob.mob.tickCount > catSearchTick_) {
        catSearchTick_ = mob.mob.tickCount + 20;
        scaredOfCat_ = false;
        for (ServerEntity* other : mob.entities) {
            if (other->removed || other->dying || other->type().path() != "cat" ||
                std::abs(other->x - mob.mob.x) > 16.0 + (mob.mob.width() / 2.0) ||
                std::abs(other->y - mob.mob.y) > 16.0 + mob.mob.height() ||
                std::abs(other->z - mob.mob.z) > 16.0 + (mob.mob.width() / 2.0)) {
                continue;
            }
            mob.actions.mobSound(*other, "entity.cat.hiss");
            scaredOfCat_ = true;
        }
    }
    return !scaredOfCat_;
}

void PhantomSweepGoal::stop(MobContext& mob) {
    mob.mob.targetId.reset();
    mob.mob.flight.swooping = false;
}

void PhantomSweepGoal::tick(MobContext& mob) {
    const std::optional<TargetView> target = targetOf(mob);
    if (!target.has_value()) {
        return;
    }
    mob.mob.flight.moveTargetPoint = {target->x, target->y + (target->height * 0.5), target->z};
    // parity: its box grown by 0.2 against the target's.
    const double reachX = (mob.mob.width() / 2.0) + 0.2 + (target->width / 2.0);
    const bool touching = std::abs(target->x - mob.mob.x) < reachX && std::abs(target->z - mob.mob.z) < reachX &&
                          mob.mob.y - 0.2 < target->y + target->height &&
                          mob.mob.y + mob.mob.height() + 0.2 > target->y;
    if (touching) {
        mob.actions.swing(mob.mob);
        const float damage = entityCombat::attackDamageOf(mob.mob);
        if (target->playerId.has_value()) {
            mob.actions.hurtPlayer(mob.mob, *target->playerId, damage);
        } else if (target->entity != nullptr) {
            mob.actions.hurtMob(mob.mob, *target->entity, damage);
        }
        mob.mob.flight.swooping = false;
        mob.actions.mobSound(mob.mob, "entity.phantom.bite");
    } else if (mob.mob.horizontallyBlocked || mob.mob.hurtCooldown > 10) {
        mob.mob.flight.swooping = false;
    }
}

bool PhantomCircleGoal::canUse(MobContext& mob) { return !mob.mob.targetId.has_value() || !mob.mob.flight.swooping; }

void PhantomCircleGoal::start(MobContext& mob) {
    distance_ = 5.0F + (mob.random.nextFloat() * 10.0F);
    height_ = -4.0F + (mob.random.nextFloat() * 9.0F);
    clockwise_ = mob.random.nextBoolean() ? 1.0F : -1.0F;
    selectNext(mob);
}

void PhantomCircleGoal::tick(MobContext& mob) {
    if (mob.random.nextInt(adjustedTickDelay(350)) == 0) {
        height_ = -4.0F + (mob.random.nextFloat() * 9.0F);
    }
    if (mob.random.nextInt(adjustedTickDelay(250)) == 0) {
        ++distance_;
        if (distance_ > 15.0F) {
            distance_ = 5.0F;
            clockwise_ = -clockwise_;
        }
    }
    if (mob.random.nextInt(adjustedTickDelay(450)) == 0) {
        angle_ = mob.random.nextFloat() * 2.0F * std::numbers::pi_v<float>;
        selectNext(mob);
    }
    if (touchingTarget(mob.mob)) {
        selectNext(mob);
    }
    const Block at = blockOf(mob.mob);
    if (mob.mob.flight.moveTargetPoint[1] < mob.mob.y && !isAir(mob, {at[0], at[1] - 1, at[2]})) {
        height_ = std::max(1.0F, height_);
        selectNext(mob);
    }
    if (mob.mob.flight.moveTargetPoint[1] > mob.mob.y && !isAir(mob, {at[0], at[1] + 1, at[2]})) {
        height_ = std::min(-1.0F, height_);
        selectNext(mob);
    }
}

void PhantomCircleGoal::selectNext(MobContext& mob) {
    if (!anchorOf(mob.mob).has_value()) {
        setAnchor(mob.mob, blockOf(mob.mob));
    }
    const Block anchor = *anchorOf(mob.mob);
    angle_ += clockwise_ * 15.0F * kRadians;
    mob.mob.flight.moveTargetPoint = {anchor[0] + static_cast<double>(distance_ * mth::cos(angle_)),
                                      anchor[1] + static_cast<double>(-4.0F + height_),
                                      anchor[2] + static_cast<double>(distance_ * mth::sin(angle_))};
}

bool PhantomTargetGoal::canUse(MobContext& mob) {
    if (nextScanTick_ > 0) {
        --nextScanTick_;
        return false;
    }
    nextScanTick_ = reducedTickDelay(60);
    // parity: `getNearbyPlayers` in a box sixteen across and sixty-four up, highest first.
    std::vector<const PlayerView*> near;
    const double halfWidth = mob.mob.width() / 2.0;
    for (const PlayerView& player : mob.players) {
        if (!player.targetable || std::abs(player.x - mob.mob.x) > 16.0 + halfWidth + (player.width / 2.0) ||
            std::abs(player.z - mob.mob.z) > 16.0 + halfWidth + (player.width / 2.0) ||
            player.y > mob.mob.y + mob.mob.height() + 64.0 || player.y + player.height < mob.mob.y - 64.0) {
            continue;
        }
        const double dx = player.x - mob.mob.x;
        const double dy = player.y - mob.mob.y;
        const double dz = player.z - mob.mob.z;
        if ((dx * dx) + (dy * dy) + (dz * dz) > 64.0 * 64.0) {
            continue;
        }
        near.push_back(&player);
    }
    if (near.empty()) {
        return false;
    }
    std::ranges::stable_sort(near, [](const PlayerView* a, const PlayerView* b) { return a->y > b->y; });
    mob.mob.targetId = near.front()->entityId;
    return true;
}

bool PhantomTargetGoal::canContinueToUse(MobContext& mob) { return canAttack(mob); }

} // namespace basalt::server::runtime::entity::ai
