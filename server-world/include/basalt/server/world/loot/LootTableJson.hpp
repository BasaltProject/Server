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

struct LootTableParseError {
    std::string message;
};

/// parity: 26.3 made conditions, modifiers and numbers registry holders, so a file may name an entry.
class LootReferences {
public:
    using ReadEntry =
        std::function<std::optional<std::string>(std::string_view registry, const wire::common::Identifier& id)>;
    using Entry = std::variant<LootCondition, std::vector<LootFunction>, IntProvider, FloatProvider>;

    explicit LootReferences(ReadEntry read) : read_(std::move(read)) {}

    [[nodiscard]] std::optional<std::string> read(std::string_view registry, const wire::common::Identifier& id) const;
    [[nodiscard]] std::optional<Entry> cached(const std::string& key) const;
    void remember(const std::string& key, Entry entry);

    void replace(ReadEntry read);

private:
    ReadEntry read_;
    mutable std::mutex mutex_;
    std::map<std::string, Entry> cache_;
};

namespace lootTableJson {

[[nodiscard]] std::expected<LootTable, LootTableParseError> parse(std::string_view json,
                                                                  LootReferences* references = nullptr);

[[nodiscard]] std::expected<LootCondition, LootTableParseError> parseCondition(const wire::common::JsonValue& json,
                                                                               LootReferences* references = nullptr);

[[nodiscard]] std::expected<std::vector<LootFunction>, LootTableParseError>
parseModifier(const wire::common::JsonValue& json, LootReferences* references = nullptr);

[[nodiscard]] std::expected<IntProvider, LootTableParseError> parseIntProvider(const wire::common::JsonValue& json,
                                                                               LootReferences* references = nullptr);

[[nodiscard]] std::expected<FloatProvider, LootTableParseError>
parseFloatProvider(const wire::common::JsonValue& json, LootReferences* references = nullptr);

} // namespace lootTableJson

} // namespace basalt::server::world::loot
