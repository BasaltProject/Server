#pragma once

#include <basalt/server/world/loot/Loot.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <expected>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace basalt::wire::common {
class JsonValue;
}

namespace basalt::server::world::loot {

/// Why loot data did not parse: a type this build does not understand, or a reference it could not
/// resolve, not only malformed JSON.
struct LootTableParseError {
    std::string message;
};

/// The named entries loot data refers to by id: `predicate`, `item_modifier`,
/// `context_int_provider` and `context_float_provider`.
///
/// parity: 26.3 made every condition, modifier and number a registry holder, so a file may name an
/// entry instead of writing it out. The registries are fixed once the data is loaded, so an entry is
/// resolved into its value the first time it is named and shared from then on. Several threads may
/// parse at once, so the cache has one owner and it is this lock.
class LootReferences {
public:
    /// One registry entry's JSON, `registry` being the folder name under `data/<namespace>/`.
    using ReadEntry =
        std::function<std::optional<std::string>(std::string_view registry, const wire::common::Identifier& id)>;
    using Entry = std::variant<LootCondition, std::vector<LootFunction>, IntProvider, FloatProvider>;

    explicit LootReferences(ReadEntry read) : read_(std::move(read)) {}

    [[nodiscard]] std::optional<std::string> read(std::string_view registry, const wire::common::Identifier& id) const;
    [[nodiscard]] std::optional<Entry> cached(const std::string& key) const;
    void remember(const std::string& key, Entry entry);

private:
    ReadEntry read_;
    mutable std::mutex mutex_;
    std::map<std::string, Entry> cache_;
};

/// Parses the vanilla 26.3 loot data formats. Only the entry, condition and function types the
/// evaluator itself implements are recognised; everything else is refused by name rather than
/// silently producing wrong loot, so the gap is visible instead of a subtly incorrect drop table.
/// Without `references`, data that names a registry entry is refused the same way.
namespace lootTableJson {

[[nodiscard]] std::expected<LootTable, LootTableParseError> parse(std::string_view json,
                                                                  LootReferences* references = nullptr);

/// A `predicate` holder: an inline condition or the id of one.
[[nodiscard]] std::expected<LootCondition, LootTableParseError> parseCondition(const wire::common::JsonValue& json,
                                                                               LootReferences* references = nullptr);

/// An `item_modifier` holder: an inline function, a list of them, or the id of one; flattened.
[[nodiscard]] std::expected<std::vector<LootFunction>, LootTableParseError>
parseModifier(const wire::common::JsonValue& json, LootReferences* references = nullptr);

/// A `context_int_provider` holder: a number, an inline provider, or the id of one.
[[nodiscard]] std::expected<IntProvider, LootTableParseError> parseIntProvider(const wire::common::JsonValue& json,
                                                                               LootReferences* references = nullptr);

/// A `context_float_provider` holder, likewise.
[[nodiscard]] std::expected<FloatProvider, LootTableParseError>
parseFloatProvider(const wire::common::JsonValue& json, LootReferences* references = nullptr);

} // namespace lootTableJson

} // namespace basalt::server::world::loot
