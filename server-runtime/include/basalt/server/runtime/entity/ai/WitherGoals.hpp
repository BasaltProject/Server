#pragma once

#include <basalt/server/runtime/entity/ai/Goal.hpp>

namespace basalt::server::runtime::entity::ai {

/// parity: `WitherBoss.WitherDoNothingGoal`.
class WitherDoNothingGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move, GoalFlag::Jump, GoalFlag::Look}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;
};

class WitherTargetGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Target}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;
};

} // namespace basalt::server::runtime::entity::ai
