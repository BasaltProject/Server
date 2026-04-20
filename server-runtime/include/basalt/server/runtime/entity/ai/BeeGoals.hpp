#pragma once

namespace basalt::server::runtime::entity::ai {

class GoalSelector;

/// parity: `Bee.registerGoals`, priority for priority.
namespace beeGoals {

void registerGoals(GoalSelector& selector);

} // namespace beeGoals

} // namespace basalt::server::runtime::entity::ai
