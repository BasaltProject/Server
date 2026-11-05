#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/entity/brain/BrainBehaviors.hpp>
#include <basalt/server/runtime/entity/brain/BrainState.hpp>

namespace basalt::server::runtime::entity::brain {

void runBehaviors(ai::MobContext& context, BrainState& state) {
    if (!state.hasBehaviors() || !state.claimRun(context.gameTime)) {
        return;
    }
    const std::vector<BehaviorRow> rows = state.behaviors();
    using ai::PluginGoalPhase;
    int budget = kBehaviorCallBudget;
    const auto call = [&](const BehaviorRow& row, PluginGoalPhase phase) {
        --budget;
        return context.actions.pluginBehavior(context.mob, row.key, phase);
    };
    for (const BehaviorRow& row : rows) {
        if (budget <= 0) {
            return;
        }
        if (state.isActive(row.activity) && !state.isRunning(row.key) && call(row, PluginGoalPhase::CanUse)) {
            state.setRunning(row.key, true);
            call(row, PluginGoalPhase::Start);
        }
    }
    for (const BehaviorRow& row : rows) {
        if (!state.isRunning(row.key)) {
            continue;
        }
        if (budget <= 0) {
            return;
        }
        if (state.isActive(row.activity) && call(row, PluginGoalPhase::CanContinue)) {
            call(row, PluginGoalPhase::Tick);
        } else {
            state.setRunning(row.key, false);
            call(row, PluginGoalPhase::Stop);
        }
    }
}

} // namespace basalt::server::runtime::entity::brain
