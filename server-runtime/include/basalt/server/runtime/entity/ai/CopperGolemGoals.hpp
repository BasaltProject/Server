#pragma once

#include <basalt/server/runtime/entity/ai/Goal.hpp>

#include <cstdint>
#include <optional>

namespace basalt::server::runtime::entity::ai {

/// parity: `TransportItemsBetweenContainers`.
class CopperGolemTransportGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move, GoalFlag::Look}; }

    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;
};

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

/// parity: `SetEntityLookTargetSometimes(PLAYER, 6, 40..80)`.
class LookAtPlayerSometimesGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Look}; }

    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    std::int32_t ticksUntilNextStart_ = 0;
    std::int32_t lookTicks_ = 0;
    std::optional<std::int32_t> player_;
};

} // namespace basalt::server::runtime::entity::ai
