#pragma once

#include <basalt/server/runtime/entity/ai/CatGoals.hpp>
#include <basalt/server/runtime/entity/ai/Goals.hpp>

namespace basalt::server::runtime::entity::ai {

/// parity: `Turtle.TurtleLayEggGoal`.
class TurtleLayEggGoal final : public MoveToBlockGoal {
public:
    explicit TurtleLayEggGoal(double speedModifier);

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void tick(MobContext& mob) override;

protected:
    [[nodiscard]] bool isValidTarget(MobContext& mob, std::int32_t x, std::int32_t y, std::int32_t z) override;
};

/// parity: `Turtle.TurtleGoToWaterGoal`.
class TurtleGoToWaterGoal final : public MoveToBlockGoal {
public:
    explicit TurtleGoToWaterGoal(double speedModifier);

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

protected:
    [[nodiscard]] bool shouldRecalculatePath() const override;

    [[nodiscard]] bool isValidTarget(MobContext& mob, std::int32_t x, std::int32_t y, std::int32_t z) override;

private:
    double speedModifier_;
};

/// parity: `Turtle.TurtleGoHomeGoal`.
class TurtleGoHomeGoal final : public Goal {
public:
    explicit TurtleGoHomeGoal(double speedModifier) : speedModifier_(speedModifier) {}

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    double speedModifier_;
    bool stuck_ = false;
    std::int32_t closeToHomeTryTicks_ = 0;
};

/// parity: `Turtle.TurtleTravelGoal`.
class TurtleTravelGoal final : public Goal {
public:
    explicit TurtleTravelGoal(double speedModifier) : speedModifier_(speedModifier) {}

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    double speedModifier_;
    bool stuck_ = false;
};

/// parity: `Turtle.TurtleRandomStrollGoal`.
class TurtleRandomStrollGoal final : public RandomStrollGoal {
public:
    TurtleRandomStrollGoal(double speedModifier, std::int32_t interval) : RandomStrollGoal(speedModifier, interval) {}

    [[nodiscard]] bool canUse(MobContext& mob) override;
};

} // namespace basalt::server::runtime::entity::ai
