#pragma once

#include <basalt/server/runtime/entity/ai/Goals.hpp>

#include <cstdint>
#include <set>

namespace basalt::server::runtime::entity::ai {

/// parity: `Ghast.RandomFloatAroundGoal`: a point picked within sixteen blocks across and a few up
/// or down, floated to and then forgotten. It is the whole of a ghast's idle life.
class FlyingWanderGoal final : public Goal {
public:
    /// `gated` is parity: `GhastMoveControl.floatDuration`, which pushes every few ticks rather than
    /// every one.
    explicit FlyingWanderGoal(std::int32_t range = 16, double step = 0.1, bool gated = true);

    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move}; }

    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    std::int32_t range_;
    double step_;
    bool gated_;
    double wantedX_ = 0.0;
    double wantedY_ = 0.0;
    double wantedZ_ = 0.0;
    bool arrived_ = true;
    std::int32_t floatDuration_ = 0;
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

/// parity: `Phantom`'s circle and swoop: it climbs to a point above whatever it is after, then dives
/// through it and pulls up again.
class SwoopAttackGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move, GoalFlag::Look}; }

    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    bool diving_ = false;
};

/// parity: `Vex.VexChargeAttackGoal`: it picks a point past you and flies through it.
class ChargeAttackGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move, GoalFlag::Look}; }

    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

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
