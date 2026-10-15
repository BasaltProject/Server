#pragma once

#include <basalt/server/runtime/entity/ai/Goal.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <set>

namespace basalt::server::runtime::entity::ai {

/// parity: `RunAroundLikeCrazyGoal`.
class RunAroundLikeCrazyGoal final : public Goal {
public:
    explicit RunAroundLikeCrazyGoal(double speedModifier);

    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    double speedModifier_;
    std::optional<std::array<double, 3>> wanted_;
};

/// parity: `RandomStandGoal`.
class RandomStandGoal final : public Goal {
public:
    RandomStandGoal();

    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& /*mob*/) override { return false; }

    void start(MobContext& mob) override;

private:
    std::int32_t nextStand_;
};

} // namespace basalt::server::runtime::entity::ai
