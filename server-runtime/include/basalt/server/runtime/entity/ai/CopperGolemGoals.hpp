#pragma once

#include <basalt/server/runtime/entity/ai/Goal.hpp>

namespace basalt::server::runtime::entity::ai {

/// parity: `TransportItemsBetweenContainers` as `CopperGolemAi` configures it: from a copper chest
/// to a plain one, sixteen at a time, a minute at each chest.
class CopperGolemTransportGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move, GoalFlag::Look}; }

    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;
};

/// parity: the idle `RunOne` of `RandomStroll.stroll(1.0, 2, 2)` and `DoNothing(30, 60)`, which only
/// runs while the transport is cooling down.
class CopperGolemIdleGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move}; }

    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void tick(MobContext& mob) override;

    void stop(MobContext& mob) override;
};

} // namespace basalt::server::runtime::entity::ai
