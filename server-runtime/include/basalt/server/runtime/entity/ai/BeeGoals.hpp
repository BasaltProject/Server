#pragma once

namespace basalt::server::runtime::entity::ai {

class GoalSelector;

/// The bee's goals: its sting, the hive it goes home to, the flowers it pollinates and the crops it
/// grows on the way back.
///
/// parity: `Bee.registerGoals`, priority for priority, less `ResetUniversalAngerTargetGoal`, which
/// only does anything with the `universal_anger` rule on.
namespace beeGoals {

void registerGoals(GoalSelector& selector);

} // namespace beeGoals

} // namespace basalt::server::runtime::entity::ai
