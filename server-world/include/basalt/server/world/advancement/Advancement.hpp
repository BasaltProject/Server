#pragma once

#include <basalt/server/world/loot/LootCondition.hpp>
#include <basalt/server/world/loot/LootPredicates.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/text/Component.hpp>

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::world::advancement {

/// One `items` entry of an item predicate: a set of ids, or a tag standing for one.
struct ItemMatcher {
    std::vector<wire::common::Identifier> items;
    std::optional<wire::common::Identifier> tag;
};

/// parity: `CriterionTriggerInstance`. A few triggers keep a shape of their own and the rest are read
/// field by field; a criterion that asks for something unread is `Unsupported` and never fires.
enum class ConditionKind : std::uint8_t {
    InventoryItems,
    RecipeUnlocked,
    /// parity: `LootTableTrigger` as `player_generates_container_loot`.
    ContainerLoot,
    /// parity: `ChangeDimensionTrigger`.
    ChangedDimension,
    /// parity: `RecipeCraftedTrigger`, the recipe behind a result the player took.
    RecipeCrafted,
    /// Read field by field as 26.3 writes it: see `GenericCondition`.
    Generic,
    Unsupported,
};

/// parity: `DamageSourcePredicate`: tags the damage type carries or lacks, and who dealt it.
struct DamageSourceCheck {
    std::vector<std::pair<std::string, bool>> tags;
    std::shared_ptr<const loot::EntityPredicate> direct;
    std::shared_ptr<const loot::EntityPredicate> source;
    std::optional<bool> isDirect;
};

/// parity: `DamagePredicate`: how much, whether a shield took it, and the source.
struct DamageCheck {
    loot::DoubleBounds dealt;
    loot::DoubleBounds taken;
    std::shared_ptr<const loot::EntityPredicate> source;
    std::optional<bool> blocked;
    std::optional<DamageSourceCheck> type;
};

/// A criterion's conditions by field, as 26.3 serializes every trigger instance: context predicates
/// on the entities involved, item predicates, bounds and the few shapes of their own.
struct GenericCondition {
    /// `player`, `entity`, `source`, `projectile` and the rest: each a list of loot conditions run
    /// with that entity as `this` and the player's position as the origin.
    std::map<std::string, std::vector<loot::LootCondition>, std::less<>> entities;
    /// parity: `KilledByArrowTrigger` and `ChanneledLightningTrigger`, each predicate its own victim.
    std::vector<std::vector<loot::LootCondition>> victims;
    std::map<std::string, loot::ItemPredicate, std::less<>> items;
    std::map<std::string, loot::DoubleBounds, std::less<>> numbers;
    /// Ids or one `#tag`, by field: `recipes`, `potion`, `block`.
    std::map<std::string, std::vector<std::string>, std::less<>> ids;
    std::optional<loot::StateMatcher> state;
    std::optional<loot::DistancePredicate> distance;
    std::shared_ptr<const loot::LocationPredicate> startPosition;
    std::vector<loot::EffectRequirement> effects;
    std::optional<DamageCheck> damage;
    std::optional<DamageSourceCheck> killingBlow;
    /// parity: `RecipeCraftedTrigger.ingredients`, each predicate a different ingredient.
    std::vector<loot::ItemPredicate> ingredients;
    /// The `location` predicate of the block triggers, run with the block as the loot context.
    std::optional<std::vector<loot::LootCondition>> location;
};

/// What a criterion is waiting for. One tagged struct rather than a hierarchy: without RTTI the kind
/// is the tag, and the condition is data the loader reads.
struct CriterionCondition {
    ConditionKind kind = ConditionKind::Unsupported;
    /// `minecraft:inventory_changed`: every matcher must be satisfied by something in the inventory.
    std::vector<ItemMatcher> matchers;
    /// `minecraft:recipe_unlocked` and `minecraft:recipe_crafted`: the recipe learnt or used.
    std::optional<wire::common::Identifier> recipe;
    /// `minecraft:player_generates_container_loot`: the table that was rolled.
    std::optional<wire::common::Identifier> lootTable;
    /// `minecraft:changed_dimension`: where the player came from and went to, either optional.
    std::optional<wire::common::Identifier> fromDimension;
    std::optional<wire::common::Identifier> toDimension;
    std::shared_ptr<const GenericCondition> generic;
};

/// One named criterion: which trigger grants it, and under what condition.
struct AdvancementCriterion {
    wire::common::Identifier trigger;
    CriterionCondition condition;
};

/// AND-of-OR criterion groups. parity: `AdvancementRequirements.test`: every group must have at least
/// one of its criteria granted, and an advancement with no groups is never complete.
struct AdvancementRequirements {
    std::vector<std::vector<std::string>> groups;

    [[nodiscard]] bool isComplete(const std::set<std::string, std::less<>>& granted) const;
};

struct AdvancementRewards {
    std::int32_t experience = 0;
    std::vector<wire::common::Identifier> loot;
    std::vector<wire::common::Identifier> recipes;
};

/// What a completed advancement says in chat. parity: `DisplayInfo` plus `AdvancementType`, of which
/// only the parts an announcement needs are kept: nothing here draws the advancement screen.
struct AdvancementDisplay {
    wire::text::Component title;
    wire::text::Component description;
    /// `task`, `goal` or `challenge`, which decides the message key and the colour.
    std::string type = "task";
    bool announceToChat = true;
};

/// One advancement definition. parity: vanilla's `Advancement` record. The criteria keep the order
/// the file wrote them in, because a missing `requirements` is built straight out of it.
struct Advancement {
    std::optional<wire::common::Identifier> parent;
    std::vector<std::pair<std::string, AdvancementCriterion>> criteria;
    AdvancementRequirements requirements;
    AdvancementRewards rewards;
    std::optional<AdvancementDisplay> display;

    [[nodiscard]] const AdvancementCriterion* criterion(std::string_view name) const;

    [[nodiscard]] bool isComplete(const std::set<std::string, std::less<>>& granted) const {
        return requirements.isComplete(granted);
    }
};

} // namespace basalt::server::world::advancement
