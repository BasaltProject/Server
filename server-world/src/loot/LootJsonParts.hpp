#pragma once

#include <basalt/server/world/loot/Loot.hpp>
#include <basalt/server/world/loot/LootTableJson.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// The pieces of the loot table reader that do not recurse into pools and entries.
namespace basalt::server::world::loot::lootTableJson::parts {

/// A parse either produced the value or named what it could not read.
template <typename Value>
using Parsed = std::expected<Value, LootTableParseError>;

[[nodiscard]] LootTableParseError missing(std::string_view what);
[[nodiscard]] const wire::common::JsonObject* asObject(const wire::common::JsonValue* value);
[[nodiscard]] const std::vector<wire::common::JsonValue>* asArray(const wire::common::JsonValue* value);
[[nodiscard]] std::optional<std::string> asString(const wire::common::JsonValue* value);
/// A JSON number as read by a schema that also accepts it written as a string.
[[nodiscard]] std::optional<double> asNumber(const wire::common::JsonValue* value);
[[nodiscard]] std::optional<bool> asBool(const wire::common::JsonValue* value);
/// An id list, a single id, or a `#tag`, as a registry holder set is written.
[[nodiscard]] std::vector<std::string> idsOrTag(const wire::common::JsonValue* value);
[[nodiscard]] ItemPredicate parseItemPredicate(const wire::common::JsonObject& object);
[[nodiscard]] EntityPredicate parseEntityPredicate(const wire::common::JsonObject& object);
[[nodiscard]] LevelBasedValue parseLevelBased(const wire::common::JsonValue* value);
/// `set_components`, `set_name`, `set_potion` and `set_ominous_bottle_amplifier`.
[[nodiscard]] Parsed<LootFunction> parseComponentSetter(const wire::common::JsonObject& function);

} // namespace basalt::server::world::loot::lootTableJson::parts
