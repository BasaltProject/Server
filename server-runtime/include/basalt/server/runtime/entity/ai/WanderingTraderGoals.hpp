#pragma once

#include <basalt/server/runtime/entity/ai/Goals.hpp>

#include <array>
#include <optional>
#include <set>

namespace basalt::server::runtime::entity::ai {

/// parity: `WanderingTrader.WanderToPositionGoal`.
class WanderingTraderWanderGoal final : public Goal {
public:
    explicit WanderingTraderWanderGoal(double stopDistance = 2.0, double speedModifier = 0.35);

    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move}; }

    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    double stopDistance_;
    double speedModifier_;
};

/// parity: `MoveTowardsRestrictionGoal` for the transient sixteen-block home the spawner installs.
class MoveTowardsTraderHomeGoal final : public Goal {
public:
    explicit MoveTowardsTraderHomeGoal(double speedModifier = 0.35);

    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

private:
    double speedModifier_;
    std::optional<std::array<double, 3>> wanted_;
};

/// parity: `UseItemGoal` as the wandering trader sets it up: an invisibility potion at night, milk by day.
class TraderUseItemGoal final : public Goal {
public:
    explicit TraderUseItemGoal(bool potion) : potion_(potion) {}

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    bool potion_;
    std::int32_t remaining_ = 0;
};

} // namespace basalt::server::runtime::entity::ai
