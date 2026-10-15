#pragma once

#include <basalt/server/world/loot/LootContext.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <array>
#include <optional>

namespace basalt::server::runtime::world {

class LoadedWorld;
class RealLootTables;

/// parity: `predicate=` and `execute if predicate`.
namespace commandPredicates {

/// Nothing when no pack declares the id or the condition is unreadable, which vanilla answers as false.
[[nodiscard]] std::optional<bool> test(RealLootTables& tables, const wire::common::JsonValue& predicate,
                                       LoadedWorld* world, const std::array<double, 3>& origin,
                                       std::optional<server::world::loot::EntityFacts> self);

} // namespace commandPredicates

} // namespace basalt::server::runtime::world
