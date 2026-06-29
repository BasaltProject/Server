#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/entity/ai/PluginGoal.hpp>

#include <utility>

namespace basalt::server::runtime::entity::ai {

PluginGoal::PluginGoal(std::string key, std::set<GoalFlag> flags) : key_(std::move(key)), flags_(std::move(flags)) {}

bool PluginGoal::canUse(MobContext& mob) { return mob.actions.pluginGoal(mob.mob, key_, PluginGoalPhase::CanUse); }

bool PluginGoal::canContinueToUse(MobContext& mob) {
    return mob.actions.pluginGoal(mob.mob, key_, PluginGoalPhase::CanContinue);
}

void PluginGoal::start(MobContext& mob) { mob.actions.pluginGoal(mob.mob, key_, PluginGoalPhase::Start); }

void PluginGoal::stop(MobContext& mob) { mob.actions.pluginGoal(mob.mob, key_, PluginGoalPhase::Stop); }

void PluginGoal::tick(MobContext& mob) { mob.actions.pluginGoal(mob.mob, key_, PluginGoalPhase::Tick); }

} // namespace basalt::server::runtime::entity::ai
