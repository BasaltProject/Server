#pragma once

#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/text/Component.hpp>

#include <cstdint>
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

/// A block as a criterion names it: which blocks count, and the state values that must match.
///
/// parity: `BlockPredicate` cut down to what the criteria in the jar actually ask for, which is a
/// set of blocks or one tag, plus a handful of property values.
struct BlockMatcher {
    std::vector<wire::common::Identifier> blocks;
    std::optional<wire::common::Identifier> tag;
    std::vector<std::pair<std::string, std::string>> properties;
};

/// parity: `CriterionTriggerInstance`. Deliberately incomplete: a trigger with no case here is
/// `Unsupported` and never fires, which is the honest answer while the gameplay event behind it does
/// not exist. The two that do exist cover 3299 of vanilla's 3500-odd criteria.
enum class ConditionKind : std::uint8_t {
    InventoryItems,
    RecipeUnlocked,
    /// parity: `KilledTrigger` as `player_killed_entity`.
    KilledEntity,
    /// parity: `TameAnimalTrigger`.
    TamedAnimal,
    /// parity: `SummonedEntityTrigger`.
    SummonedEntity,
    /// parity: `LootTableTrigger` as `player_generates_container_loot`.
    ContainerLoot,
    /// parity: `ChangeDimensionTrigger`.
    ChangedDimension,
    /// parity: `ConsumeItemTrigger`.
    ConsumedItem,
    /// parity: `FilledBucketTrigger`.
    FilledBucket,
    /// parity: `BredAnimalsTrigger`, which names the newborn or the two parents.
    BredAnimals,
    /// parity: `EnterBlockTrigger`, the block a player is standing in.
    EnteredBlock,
    /// parity: `PlayerTrigger` as `location`: where the player is standing, every second.
    AtLocation,
    /// parity: `RecipeCraftedTrigger`, the recipe behind a result the player took.
    RecipeCrafted,
    /// parity: `FishingRodHookedTrigger`, what came out of the water.
    FishingHooked,
    /// parity: `PlayerInteractTrigger` as `player_interacted_with_entity`.
    InteractedWithEntity,
    /// parity: `ItemUsedOnLocationTrigger` as `placed_block`: the block that is now there.
    PlacedBlock,
    /// parity: `ItemUsedOnLocationTrigger` as `item_used_on_block`: what was used on what.
    ItemUsedOnBlock,
    /// parity: `PlayerTrigger` as `slept_in_bed`.
    SleptInBed,
    /// parity: `TradeTrigger`, a villager's offer taken.
    VillagerTrade,
    /// parity: `EnchantedItemTrigger`.
    EnchantedItem,
    /// parity: `ConstructBeaconTrigger`, the tiers under a beacon that just lit.
    ConstructedBeacon,
    Unsupported,
};

/// What a criterion is waiting for. One tagged struct rather than a hierarchy: without RTTI the kind
/// is the tag, and the condition is data the loader reads.
struct CriterionCondition {
    ConditionKind kind = ConditionKind::Unsupported;
    /// `minecraft:inventory_changed`: every matcher must be satisfied by something in the inventory.
    std::vector<ItemMatcher> matchers;
    /// `minecraft:recipe_unlocked` and `minecraft:recipe_crafted`: the recipe learnt or used.
    std::optional<wire::common::Identifier> recipe;
    /// The entity types a kill, a taming, a summon or a newborn asks for, ids or a single `#tag`.
    /// Empty is any.
    std::vector<std::string> entityTypes;
    /// Whether the entity has to be a baby or a grown one. parity: the `minecraft:flags` predicate,
    /// of which only `is_baby` is read.
    std::optional<bool> baby;
    /// `minecraft:bred_animals`: the two parents, which the trigger matches either way round.
    std::vector<std::string> parentTypes;
    std::vector<std::string> partnerTypes;
    /// `minecraft:player_generates_container_loot`: the table that was rolled.
    std::optional<wire::common::Identifier> lootTable;
    /// `minecraft:construct_beacon`: how many tiers the beacon stands on, either end optional.
    std::optional<std::int32_t> minLevel;
    std::optional<std::int32_t> maxLevel;
    /// `minecraft:placed_block` and `minecraft:item_used_on_block`: the block it happened on.
    std::optional<BlockMatcher> blockMatcher;
    /// `minecraft:enter_block`: the block the player has to be standing in.
    std::optional<wire::common::Identifier> block;
    /// `minecraft:location`: the biome or the structure the player has to be standing in.
    std::optional<wire::common::Identifier> biome;
    std::optional<wire::common::Identifier> structure;
    /// `minecraft:changed_dimension`: where the player came from and went to, either optional.
    std::optional<wire::common::Identifier> fromDimension;
    std::optional<wire::common::Identifier> toDimension;
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
