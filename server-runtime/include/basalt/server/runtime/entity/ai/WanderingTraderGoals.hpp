#pragma once

#include <basalt/server/runtime/entity/ai/Goals.hpp>

#include <array>
#include <optional>
#include <set>

namespace basalt::server::runtime::entity::ai {

/// parity: `WanderingTrader.WanderToPositionGoal`, driven by the meeting point its own spawner picks.
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

} // namespace basalt::server::runtime::entity::ai
