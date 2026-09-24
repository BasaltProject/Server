#pragma once

#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <array>
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
    /// parity: `Entity.getScoreboardName`, a player's name or anything else's UUID.
    std::optional<std::string> scoreName;
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
    /// parity: `minecraft:stored_enchantments`, the same test over what a book stores.
    std::vector<EnchantmentRequirement> storedEnchantments;
    /// parity: `AnyValue`, a component type with no predicate of its own only has to be there.
    std::vector<std::string> present;
    /// Set when the predicate asks something this build does not model, which then never matches.
    bool unsupported = false;

    /// Without the stack's components, which is enough for everything but stored enchantments and
    /// presence, so those never match here.
    [[nodiscard]] bool test(const std::optional<wire::common::Identifier>& item,
                            const std::map<std::string, std::int32_t, std::less<>>& itemEnchantments,
                            const TagLookup& tags) const;

    [[nodiscard]] bool test(const wire::common::Identifier& item, const wire::nbt::NbtCompound& components,
                            const TagLookup& tags) const;
};

/// parity: `EntityPredicate`, the parts the loot tables use.
struct EntityPredicate {
    std::vector<std::string> types;
    std::optional<bool> onFire;
    std::optional<bool> baby;
    std::map<std::string, std::string, std::less<>> components;
    /// parity: `minecraft:predicates` over a component, `villager/variant` naming a holder set.
    std::map<std::string, std::vector<std::string>, std::less<>> componentsIn;
    std::optional<bool> sheared;
    std::optional<std::int32_t> cubeSize;
    std::optional<bool> captain;
    std::shared_ptr<EntityPredicate> vehicle;
    std::optional<ItemPredicate> mainhand;
    bool unsupported = false;

    [[nodiscard]] bool test(const std::optional<EntityFacts>& entity, const TagLookup& tags) const;
};

/// parity: `StatePropertiesPredicate`: each named property one exact value, or within a range.
///
/// A range compares the property's own values; this build knows them only as text, so it orders
/// whole numbers numerically and `false` before `true`, and an enum range is a gap that never matches.
struct StateMatcher {
    struct Property {
        std::string name;
        std::optional<std::string> exact;
        std::optional<std::string> min;
        std::optional<std::string> max;
    };

    std::vector<Property> properties;

    [[nodiscard]] bool test(const std::map<std::string, std::string>& state) const;
};

/// What a location predicate can see of one square.
struct PlaceFacts {
    /// parity: `Level.isLoaded`, which the biome, structure, smoke, light and fluid tests need.
    bool loaded = true;
    /// The exact point asked about, which `position` compares.
    std::array<double, 3> exact{};
    std::optional<wire::common::Identifier> dimension;
    std::optional<wire::common::Identifier> biome;
    std::optional<wire::common::Identifier> block;
    std::map<std::string, std::string> properties;
    /// parity: `FluidState`, its id and its `level` and `falling` properties.
    std::optional<wire::common::Identifier> fluid;
    std::map<std::string, std::string> fluidProperties;
    /// parity: `Level.getMaxLocalRawBrightness`, `Level.canSeeSky` and `CampfireBlock.isSmokeyPos`.
    std::int32_t light = 0;
    bool canSeeSky = false;
    bool smokey = false;
    /// parity: `StructureManager.getStructureWithPieceAt` over a holder set of structures.
    std::function<bool(const std::vector<std::string>& structures)> inStructure;
};

/// parity: `MinMaxBounds.Doubles`.
struct DoubleBounds {
    std::optional<double> min;
    std::optional<double> max;

    [[nodiscard]] bool test(double value) const {
        return (!min.has_value() || value >= *min) && (!max.has_value() || value <= *max);
    }
};

/// parity: `LocationPredicate`: where, which dimension, biome, structure, smoke, light, block,
/// fluid and whether the sky is in view, tested in that order.
struct LocationPredicate {
    std::optional<std::array<DoubleBounds, 3>> position;
    std::optional<wire::common::Identifier> dimension;
    /// Ids, or a single `#tag`; empty means any biome.
    std::vector<std::string> biomes;
    std::vector<std::string> structures;
    std::optional<bool> smokey;
    std::optional<std::int32_t> lightMin;
    std::optional<std::int32_t> lightMax;
    bool hasLight = false;
    bool hasBlock = false;
    std::vector<std::string> blocks;
    StateMatcher state;
    bool hasFluid = false;
    std::vector<std::string> fluids;
    StateMatcher fluidState;
    std::optional<bool> canSeeSky;
    bool unsupported = false;

    [[nodiscard]] bool test(const PlaceFacts& place, const TagLookup& tags) const;
};

/// Whether an id matches a list of ids or a `#tag`.
[[nodiscard]] bool matchesIdsOrTag(const std::vector<std::string>& wanted, std::string_view registry,
                                   const wire::common::Identifier& id, const TagLookup& tags);

} // namespace basalt::server::world::loot
