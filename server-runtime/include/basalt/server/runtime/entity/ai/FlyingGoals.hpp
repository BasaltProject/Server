#pragma once

#include <basalt/server/runtime/entity/ai/Goals.hpp>

#include <array>
#include <cstdint>
#include <set>
#include <vector>

namespace basalt::server::runtime::entity::ai {

/// parity: `Ghast.RandomFloatAroundGoal`: a point up to sixteen blocks off each way, handed to the
/// move control whenever the last one is reached, lost or over sixty blocks away.
class GhastFloatGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& /*mob*/) override { return false; }

    void start(MobContext& mob) override;
};

/// parity: `Ghast.GhastLookGoal`: `faceMovementDirection`, or at the target within 64 blocks.
class GhastLookGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Look}; }

    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& /*mob*/) override { return true; }

    void tick(MobContext& mob) override;
};

/// parity: `WaterAvoidingRandomFlyingGoal`: one roll in sixty, then of ten spots eight blocks out
/// within a quarter turn of where it faces the darkest, hovering one to three blocks over the ground
/// or failing that in the air, and a flying path there followed square by square.
class HoverWanderGoal final : public Goal {
public:
    /// `interval` is the one-in-that roll; `BeeWanderGoal` asks it every tenth of that.
    explicit HoverWanderGoal(std::int32_t interval = 120, bool whenIdle = false);

    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move}; }

    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void stop(MobContext& mob) override;

private:
    std::int32_t interval_;
    bool whenIdle_;
};

/// parity: `Ghast.GhastShootFireballGoal`: twenty ticks of charging where it stands, a fireball
/// thrown from four blocks in front of its eyes, and two seconds before the next one.
class FireballAttackGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Look}; }

    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    std::int32_t chargeTime_ = 0;
};

/// parity: `Blaze.BlazeAttackGoal`: close in, punch what is next to it, and otherwise throw three
/// small fireballs six ticks apart with five seconds between bursts.
class BlazeAttackGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move, GoalFlag::Look}; }

    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    std::int32_t attackTime_ = 0;
    std::int32_t attackStep_ = 0;
};

/// parity: `Vex.VexChargeAttackGoal`: one roll in seven when idle, the target's eyes as the point,
/// and a hit the moment the two boxes meet.
class VexChargeGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move}; }

    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;
};

/// parity: `Vex.VexRandomMoveGoal`: three tries at an empty block near where it was summoned.
class VexRandomMoveGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& /*mob*/) override { return false; }

    void tick(MobContext& mob) override;
};

/// parity: `Bat.customServerAiStep`: hang under a solid block, or flutter at a point that changes
/// one tick in thirty.
class BatFlightGoal final : public Goal {
public:
    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& /*mob*/) override { return true; }

    void tick(MobContext& mob) override;
};

/// parity: `Guardian.GuardianAttackGoal`: the beam takes eighty ticks to charge, the guardian holds
/// still while it does, and what it hits takes the damage at the end of it.
class BeamAttackGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move, GoalFlag::Look}; }

    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    std::int32_t attackTime_ = 0;
};

} // namespace basalt::server::runtime::entity::ai
