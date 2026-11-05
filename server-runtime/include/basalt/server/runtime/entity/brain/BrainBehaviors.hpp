#pragma once

namespace basalt::server::runtime::entity::ai {
class MobContext;
}

namespace basalt::server::runtime::entity::brain {

class BrainState;

/// Most plugin calls one brain makes in a tick, so a behavior table cannot starve the tick.
inline constexpr int kBehaviorCallBudget = 24;

/// parity: `Brain.tick`: start every stopped behavior of an active activity, then tick or stop the running ones.
void runBehaviors(ai::MobContext& context, BrainState& state);

} // namespace basalt::server::runtime::entity::brain
