#pragma once

#include <basalt/server/runtime/entity/ai/Goals.hpp>

#include <cstdint>
#include <set>

namespace basalt::server::runtime::entity::ai {

/// parity: `FollowFlockLeaderGoal`.
class FollowFlockLeaderGoal final : public Goal {
public:
    FollowFlockLeaderGoal() = default;

    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    std::int32_t timeToRecalcPath_ = 0;
    std::int32_t nextStartTick_ = -1;
};

} // namespace basalt::server::runtime::entity::ai
