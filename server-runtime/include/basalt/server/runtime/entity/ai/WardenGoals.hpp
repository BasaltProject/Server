#pragma once

#include <basalt/server/runtime/entity/ai/Goal.hpp>

#include <cstdint>
#include <optional>

namespace basalt::server::runtime::entity::ai {

/// parity: `WardenAi`'s investigate activity, `GoToTargetLocation(DISTURBANCE_LOCATION, 2, 0.7)`:
/// to a block beside where the noise came from, until it is within two.
class WardenInvestigateGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move, GoalFlag::Look}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    void tick(MobContext& mob) override;
};

/// parity: `TryToSniff` then `Sniffing`: stopped still for eighty-four ticks, then angrier at the
/// nearest thing it may fight when that is close, and off to see where it is.
class WardenSniffGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move, GoalFlag::Look}; }

    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;
};

} // namespace basalt::server::runtime::entity::ai
