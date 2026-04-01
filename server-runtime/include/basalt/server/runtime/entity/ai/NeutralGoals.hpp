#pragma once

#include <basalt/server/runtime/entity/ai/Goal.hpp>

#include <cstdint>
#include <optional>
#include <set>

namespace basalt::server::runtime::entity::ai {

/// parity: `NearestAttackableTargetGoal<Player>(10, true, false, this::isAngryAt)`: the nearest
/// player a neutral mob is angry at, looked for one tick in five.
class AngryAtPlayerGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Target}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

private:
    std::optional<std::int32_t> found_;
};

} // namespace basalt::server::runtime::entity::ai
