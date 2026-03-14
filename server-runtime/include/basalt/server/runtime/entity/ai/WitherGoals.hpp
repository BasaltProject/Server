#pragma once

#include <basalt/server/runtime/entity/ai/Goal.hpp>

namespace basalt::server::runtime::entity::ai {

/// parity: `WitherBoss.WitherDoNothingGoal`, which holds every other move while it is being born.
class WitherDoNothingGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move, GoalFlag::Jump, GoalFlag::Look}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;
};

/// parity: the wither's `NearestAttackableTargetGoal<LivingEntity>`, every tick and anything alive
/// that is not undead, players and mobs alike.
class WitherTargetGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Target}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;
};

} // namespace basalt::server::runtime::entity::ai
