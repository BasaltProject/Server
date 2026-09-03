#pragma once

#include <basalt/server/runtime/entity/ai/Goals.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <set>
#include <vector>

namespace basalt::server::runtime::entity {
class ServerEntity;
}

namespace basalt::server::runtime::entity::ai {

/// parity: `PatrollingMonster.LongDistancePatrolGoal`: the leader picks a point five hundred blocks
/// off and the rest of the patrol is handed the same one every time it repaths.
class LongDistancePatrolGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    [[nodiscard]] static std::vector<ServerEntity*> companions(MobContext& mob);

    [[nodiscard]] static std::optional<std::array<std::int32_t, 3>>
    intermediateTarget(MobContext& mob, const std::array<std::int32_t, 3>& target);

    static bool moveRandomly(MobContext& mob);
};

} // namespace basalt::server::runtime::entity::ai
