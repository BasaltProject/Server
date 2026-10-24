#pragma once

#include <basalt/server/runtime/entity/ai/Goal.hpp>

#include <set>
#include <string>

namespace basalt::server::runtime::entity::ai {

/// A goal whose decisions belong to a plugin; it costs nothing on a mob that has none.
class PluginGoal final : public Goal {
public:
    PluginGoal(std::string key, std::set<GoalFlag> flags);

    [[nodiscard]] std::set<GoalFlag> flags() const override { return flags_; }

    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    std::string key_;
    std::set<GoalFlag> flags_;
};

} // namespace basalt::server::runtime::entity::ai
