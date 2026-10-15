#pragma once

#include <basalt/server/runtime/entity/ai/Goal.hpp>

#include <cstdint>

namespace basalt::server::runtime::entity::ai {

/// parity: `Phantom.PhantomAttackStrategyGoal`.
class PhantomStrategyGoal final : public Goal {
public:
    [[nodiscard]] bool canUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    std::int32_t nextSweepTick_ = 0;
};

/// parity: `Phantom.PhantomSweepAttackGoal`.
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

/// parity: `Phantom.PhantomCircleAroundAnchorGoal`.
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

/// parity: `Phantom.PhantomAttackPlayerTargetGoal`.
class PhantomTargetGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Target}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

private:
    std::int32_t nextScanTick_ = reducedTickDelay(20);
};

} // namespace basalt::server::runtime::entity::ai
