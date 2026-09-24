#pragma once

#include <basalt/server/runtime/entity/ai/Goal.hpp>

#include <cstdint>

namespace basalt::server::runtime::entity::ai {

/// parity: `Phantom.PhantomAttackStrategyGoal`: circle for a while, then swoop, with the anchor
/// set twenty to forty blocks over the target and never below the sea.
class PhantomStrategyGoal final : public Goal {
public:
    [[nodiscard]] bool canUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    std::int32_t nextSweepTick_ = 0;
};

/// parity: `Phantom.PhantomSweepAttackGoal`: dive at the target's middle and pull away after the
/// hit, or when a cat is near, when it bumps into something or when it was just hurt.
class PhantomSweepGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    bool scaredOfCat_ = false;
    std::int32_t catSearchTick_ = 0;
};

/// parity: `Phantom.PhantomCircleAroundAnchorGoal`: round the anchor fifteen degrees at a time.
class PhantomCircleGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    void selectNext(MobContext& mob);

    float angle_ = 0.0F;
    float distance_ = 0.0F;
    float height_ = 0.0F;
    float clockwise_ = 0.0F;
};

/// parity: `Phantom.PhantomAttackPlayerTargetGoal`: every three seconds, the highest player within
/// sixteen blocks across and sixty-four up or down.
class PhantomTargetGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Target}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

private:
    std::int32_t nextScanTick_ = reducedTickDelay(20);
};

} // namespace basalt::server::runtime::entity::ai
