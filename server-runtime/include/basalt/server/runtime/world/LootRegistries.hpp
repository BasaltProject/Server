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

/// parity: `TagLoader.build`.
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

    [[nodiscard]] std::vector<wire::common::Identifier> tagMembers(std::string_view registry,
                                                                   const wire::common::Identifier& tag) const;

private:
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
