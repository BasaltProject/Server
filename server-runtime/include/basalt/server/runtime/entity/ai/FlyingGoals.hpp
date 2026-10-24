#pragma once

#include <basalt/server/runtime/entity/ai/Goals.hpp>

#include <array>
#include <cstdint>
#include <set>
#include <vector>

namespace basalt::server::runtime::entity::ai {

/// parity: `Ghast.RandomFloatAroundGoal`.
class GhastFloatGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& /*mob*/) override { return false; }

    void start(MobContext& mob) override;
};

/// parity: `Ghast.GhastLookGoal`.
class GhastLookGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Look}; }

    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& /*mob*/) override { return true; }

    void tick(MobContext& mob) override;
};

/// parity: `WaterAvoidingRandomFlyingGoal`.
class HoverWanderGoal final : public Goal {
public:
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

/// parity: `Ghast.GhastShootFireballGoal`.
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

/// parity: `Blaze.BlazeAttackGoal`.
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

/// parity: `Vex.VexChargeAttackGoal`.
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

/// parity: `Vex.VexRandomMoveGoal`.
class VexRandomMoveGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& /*mob*/) override { return false; }

    void tick(MobContext& mob) override;
};

/// parity: `Bat.customServerAiStep`.
class BatFlightGoal final : public Goal {
public:
    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& /*mob*/) override { return true; }

    void tick(MobContext& mob) override;
};

/// parity: `Guardian.GuardianAttackGoal`.
class BeamAttackGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move, GoalFlag::Look}; }

    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;
};

} // namespace basalt::server::runtime::entity::ai
