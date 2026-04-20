#pragma once

#include <basalt/server/runtime/entity/ai/Goal.hpp>

#include <array>
#include <cstdint>
#include <optional>

namespace basalt::server::runtime::entity::ai {

/// parity: `BreathAirGoal`.
class BreathAirGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move, GoalFlag::Look}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool isInterruptable() const override { return false; }

    void start(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    static void findAirPosition(MobContext& mob);
};

/// parity: `TryFindWaterGoal`, a stranded swimmer asking to move to the nearest water.
class TryFindWaterGoal final : public Goal {
public:
    [[nodiscard]] bool canUse(MobContext& mob) override;

    void start(MobContext& mob) override;
};

/// parity: `RandomSwimmingGoal` over `BehaviorUtils.getRandomSwimmablePos`.
class RandomSwimmingGoal final : public Goal {
public:
    RandomSwimmingGoal(double speedModifier, std::int32_t interval);

    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

private:
    double speedModifier_;
    std::int32_t interval_;
    std::array<double, 3> wanted_{};
};

/// parity: `DolphinJumpGoal`, the leap out of open water.
class DolphinJumpGoal final : public Goal {
public:
    explicit DolphinJumpGoal(std::int32_t interval);

    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move, GoalFlag::Jump}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    [[nodiscard]] bool isInterruptable() const override { return false; }

    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    std::int32_t interval_;
    bool breached_ = false;
};

/// parity: `Dolphin.DolphinSwimWithPlayerGoal`.
class DolphinSwimWithPlayerGoal final : public Goal {
public:
    explicit DolphinSwimWithPlayerGoal(double speedModifier);

    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move, GoalFlag::Look}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    double speedModifier_;
    std::optional<std::int32_t> player_;
};

} // namespace basalt::server::runtime::entity::ai
