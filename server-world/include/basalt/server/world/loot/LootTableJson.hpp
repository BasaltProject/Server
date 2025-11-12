#pragma once

#include <basalt/server/world/loot/Loot.hpp>

#include <expected>
#include <string>
#include <string_view>

namespace basalt::server::world::loot {

/// Why a loot table did not parse: a type this build does not understand, not malformed JSON.
struct LootTableParseError {
    std::string message;
};

/// Parses the real vanilla loot table JSON schema. Only the entry, condition and function types the
/// evaluator itself implements are recognised; everything else is refused by name rather than
/// silently producing wrong loot, so the gap is visible instead of a subtly incorrect drop table.
namespace lootTableJson {

[[nodiscard]] std::expected<LootTable, LootTableParseError> parse(std::string_view json);

} // namespace lootTableJson

} // namespace basalt::server::world::loot
