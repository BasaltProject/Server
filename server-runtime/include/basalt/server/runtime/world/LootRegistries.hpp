#pragma once

#include <basalt/server/world/loot/Loot.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

class RealEnchantments;

/// What a loot roll asks the registries: the enchantments an enchanting function draws from and the
/// tags a `tag` entry or `set_instrument` picks over, in the order the jar keeps them.
///
/// parity: `TagLoader.build` keeps each tag in file order, and a random pick is an index into it, so
/// the sorted tag tables the rest of the server uses would pick a different disc.
class LootRegistries {
public:
    struct Jar {
        std::function<std::vector<std::string>(std::string_view prefix)> entriesUnder;
        std::function<std::optional<std::string>(std::string_view name)> readText;
    };

    LootRegistries(const Jar& jar, const RealEnchantments& enchantments,
                   const wire::common::Logger& logger = wire::common::Logger{"LootRegistries"});

    LootRegistries(const LootRegistries&) = delete;
    LootRegistries& operator=(const LootRegistries&) = delete;

    [[nodiscard]] const server::world::loot::Enchanting& enchanting() const noexcept { return enchanting_; }

    /// A tag's members in file order, for `item`, `enchantment` or `instrument`. Empty for any other.
    [[nodiscard]] std::vector<wire::common::Identifier> tagMembers(std::string_view registry,
                                                                   const wire::common::Identifier& tag) const;

private:
    /// An enchantment's `supported_items`, `primary_items` or `exclusive_set`: ids or one `#tag`.
    struct Selector {
        std::vector<wire::common::Identifier> ids;
        std::optional<wire::common::Identifier> tag;
        bool present = false;
    };

    struct Definition {
        server::world::loot::EnchantmentEntry entry;
        Selector supported;
        Selector primary;
        Selector exclusive;
    };

    [[nodiscard]] bool matches(const Selector& selector, std::string_view registry,
                               const wire::common::Identifier& id) const;

    [[nodiscard]] const Definition* definitionOf(const wire::common::Identifier& id) const;

    [[nodiscard]] std::vector<const server::world::loot::EnchantmentEntry*>
    candidates(const std::vector<std::string>& options) const;

    std::vector<Definition> definitions_;
    std::map<wire::common::Identifier, std::size_t> indices_;
    std::map<std::string, std::map<wire::common::Identifier, std::vector<wire::common::Identifier>>, std::less<>> tags_;
    server::world::loot::Enchanting enchanting_;
};

} // namespace basalt::server::runtime::world
