#pragma once

#include <basalt/server/world/loot/LootContext.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <array>
#include <optional>

namespace basalt::server::runtime::world {

class LoadedWorld;
class RealLootTables;

/// parity: how `predicate=` and `execute if predicate` test a `minecraft:predicate`: this entity, if
/// any, and the origin, in the level the command runs in, on the level's random.
namespace commandPredicates {

/// The predicate, named by id (a JSON string) or written in place. Nothing when no pack declares
/// the id or the value is not a condition this build reads, which vanilla answers as false.
[[nodiscard]] std::optional<bool> test(RealLootTables& tables, const wire::common::JsonValue& predicate,
                                       LoadedWorld* world, const std::array<double, 3>& origin,
                                       std::optional<server::world::loot::EntityFacts> self);

} // namespace commandPredicates

} // namespace basalt::server::runtime::world
