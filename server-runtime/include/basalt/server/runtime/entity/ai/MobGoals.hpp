#pragma once

#include <basalt/wire/common/Identifier.hpp>

#include <memory>

namespace basalt::server::runtime::entity::ai {

class GoalSelector;

/// Which goals each kind of mob is born with.
///
/// parity: every mob's own `registerGoals`, priority for priority. What is missing is named in the
/// comment beside it rather than replaced by something that looks similar: a skeleton that cannot
/// shoot yet is a skeleton with no attack goal, not a skeleton that punches.
namespace mobGoals {

/// parity: `Mob.registerGoals` for this type, or nothing when this build has none for it.
[[nodiscard]] std::unique_ptr<GoalSelector> forType(const wire::common::Identifier& type);

} // namespace mobGoals

} // namespace basalt::server::runtime::entity::ai
