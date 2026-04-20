#pragma once

#include <basalt/server/runtime/entity/ai/Goal.hpp>

#include <cstdint>
#include <optional>

namespace basalt::server::runtime::entity::ai {

/// parity: `WardenAi`.
class WardenInvestigateGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move, GoalFlag::Look}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    void tick(MobContext& mob) override;
};

/// parity: `TryToSniff`.
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
