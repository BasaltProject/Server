#pragma once

#include <basalt/server/runtime/entity/ai/Goal.hpp>

#include <array>
#include <cstdint>
#include <optional>

namespace basalt::server::runtime::entity::ai {

/// parity: `Dolphin.MoveToItemGoal`.
class DolphinMoveToItemGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    std::int32_t cooldown_ = 0;
};

/// parity: `Dolphin.PlayWithItemsGoal`: whatever it holds is thrown straight back out.
class DolphinPlayWithItemsGoal final : public Goal {
public:
    [[nodiscard]] bool canUse(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;
};

/// parity: `Dolphin.DolphinSwimToTreasureGoal`.
class DolphinSwimToTreasureGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move, GoalFlag::Look}; }

    [[nodiscard]] bool isInterruptable() const override { return false; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    [[nodiscard]] bool arrived(const MobContext& mob) const;

    bool stuck_ = false;
};

/// parity: `FollowPlayerRiddenEntityGoal`.
class FollowRiddenBoatGoal final : public Goal {
public:
    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    std::optional<std::int32_t> following_;
    std::int32_t timeToRecalcPath_ = 0;
    bool ahead_ = false;
};

} // namespace basalt::server::runtime::entity::ai
