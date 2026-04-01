#pragma once

#include <basalt/server/runtime/entity/ai/Goal.hpp>

#include <cstdint>

namespace basalt::server::runtime::entity::ai {

/// parity: `Shulker.ShulkerPeekGoal`: one roll in forty with nobody to fight, the lid a third open
/// for one to three seconds.
class ShulkerPeekGoal final : public Goal {
public:
    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    std::int32_t peekTime_ = 0;
};

/// parity: `Shulker.ShulkerAttackGoal`: wide open, and a bullet every one to five and a half seconds
/// at anything within twenty blocks.
class ShulkerAttackGoal final : public Goal {
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

/// parity: `Shulker.ShulkerNearestAttackGoal`: the nearest player in a box only four blocks deep
/// along the axis it hangs on.
class ShulkerTargetGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Target}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void stop(MobContext& mob) override;
};

} // namespace basalt::server::runtime::entity::ai
