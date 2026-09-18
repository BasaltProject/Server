#pragma once

#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::world::loot {

/// What a roll knows about one of the entities behind it.
///
/// parity: the parts of `Entity` the loot tables' `EntityPredicate`s actually read.
struct EntityFacts {
    wire::common::Identifier type;
    bool onFire = false;
    bool baby = false;
    /// The data components a predicate compares, `minecraft:sheep/color` to `white`.
    std::map<std::string, std::string, std::less<>> components;
    bool sheared = false;
    std::int32_t cubeSize = 0;
    bool captain = false;
    std::optional<wire::common::Identifier> vehicle;
    /// What it holds in its main hand, and that item's enchantments by id.
    std::optional<wire::common::Identifier> mainhand;
    std::map<std::string, std::int32_t, std::less<>> mainhandEnchantments;
};

/// Whether an id is in a tag of a registry: `item`, `entity_type` or `enchantment`.
using TagLookup =
    std::function<bool(std::string_view registry, const wire::common::Identifier& id, std::string_view tag)>;

/// One `minecraft:enchantments` requirement. parity: `EnchantmentPredicate`.
struct EnchantmentRequirement {
    /// Ids, or a single `#tag`; empty means any enchantment.
    std::vector<std::string> enchantments;
    std::optional<std::int32_t> min;
    std::optional<std::int32_t> max;
};

/// parity: `ItemPredicate`, the parts the loot tables use: which items, and which enchantments.
struct ItemPredicate {
    /// Ids, or a single `#tag`; empty means any item.
    std::vector<std::string> items;
    std::vector<EnchantmentRequirement> enchantments;
    /// Set when the predicate asks something this build does not model, which then never matches.
    bool unsupported = false;

    [[nodiscard]] bool test(const std::optional<wire::common::Identifier>& item,
                            const std::map<std::string, std::int32_t, std::less<>>& itemEnchantments,
                            const TagLookup& tags) const;
};

/// parity: `EntityPredicate`, the parts the loot tables use.
struct EntityPredicate {
    std::vector<std::string> types;
    std::optional<bool> onFire;
    std::optional<bool> baby;
    std::map<std::string, std::string, std::less<>> components;
    std::optional<bool> sheared;
    std::optional<std::int32_t> cubeSize;
    std::optional<bool> captain;
    std::shared_ptr<EntityPredicate> vehicle;
    std::optional<ItemPredicate> mainhand;
    bool unsupported = false;

    [[nodiscard]] bool test(const std::optional<EntityFacts>& entity, const TagLookup& tags) const;
};

/// Whether an id matches a list of ids or a `#tag`.
[[nodiscard]] bool matchesIdsOrTag(const std::vector<std::string>& wanted, std::string_view registry,
                                   const wire::common::Identifier& id, const TagLookup& tags);

} // namespace basalt::server::world::loot
