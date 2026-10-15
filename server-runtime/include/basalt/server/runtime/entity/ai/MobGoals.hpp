#pragma once

#include <basalt/wire/common/Identifier.hpp>

#include <memory>

namespace basalt::server::runtime::entity::ai {

class GoalSelector;

/// parity: every mob's own `registerGoals`, priority for priority.
namespace mobGoals {

/// parity: `Mob.registerGoals` for this type, or nothing when this build has none for it.
[[nodiscard]] std::unique_ptr<GoalSelector> forType(const wire::common::Identifier& type);

} // namespace mobGoals

} // namespace basalt::server::runtime::entity::ai
