#pragma once

#include <basalt/server/runtime/world/PlayerProgress.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/text/Component.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// What a player just did, and which criteria that satisfies.
///
/// parity: `CriteriaTriggers` plus `PlayerAdvancements.award`, indexed once at startup so a
/// picked-up item costs a map lookup rather than a walk over three thousand criteria.
///
/// The limit is which triggers exist, not which conditions: `inventory_changed` and
/// `recipe_unlocked` are what this build can observe, and the rest arrive with the systems that
/// produce their events.
class AdvancementTriggers {
public:
    /// Which items a tag holds, for the matchers written as `#minecraft:planks`.
    using ItemTagMembers = std::function<std::set<wire::common::Identifier>(const wire::common::Identifier& tag)>;

    /// Announces a completed advancement, when the game rule and the advancement both allow it.
    using Announce = std::function<void(std::string_view player, const wire::text::Component& message)>;

    /// Grants the experience an advancement rewards.
    using AwardExperience = std::function<void(std::string_view player, std::int32_t amount)>;

    /// Teaches a recipe an advancement rewards, which is how vanilla unlocks most of the book.
    using AwardRecipe = std::function<void(std::string_view player, const wire::common::Identifier& recipe)>;

    /// Whether an entity type is in a tag, for the matchers written as `#minecraft:raiders`.
    using EntityTagged = std::function<bool(const wire::common::Identifier& type, std::string_view tag)>;

    struct Hooks {
        ItemTagMembers itemTagMembers;
        EntityTagged entityTagged;
        Announce announce;
        AwardExperience awardExperience;
        AwardRecipe awardRecipe;
    };

    AdvancementTriggers(const RealAdvancements& advancements, PlayerProgress& progress, Hooks hooks = {});

    /// parity: `InventoryChangeTrigger`, which vanilla fires on any inventory change rather than on
    /// the item that changed, so an advancement whose matcher was already satisfied still completes
    /// the first time anything else moves.
    void onInventoryChanged(std::string_view player, std::string_view uuid,
                            const std::vector<std::optional<storage::StoredItemStack>>& inventory);

    /// parity: `RecipeUnlockedTrigger`, fired when a recipe enters the book rather than when it is
    /// used.
    void onRecipeUnlocked(std::string_view player, std::string_view uuid, const wire::common::Identifier& recipe);

    /// parity: `KilledTrigger` as `player_killed_entity`: what the player killed.
    void onKilledEntity(std::string_view player, std::string_view uuid, const wire::common::Identifier& victim);

    /// parity: `TameAnimalTrigger`.
    void onTamedAnimal(std::string_view player, std::string_view uuid, const wire::common::Identifier& animal);

    /// parity: `SummonedEntityTrigger`, the golem or boss a player just built.
    void onSummonedEntity(std::string_view player, std::string_view uuid, const wire::common::Identifier& summoned);

    /// parity: `BredAnimalsTrigger`, whose two parents match either way round.
    void onBredAnimals(std::string_view player, std::string_view uuid, const wire::common::Identifier& child,
                       const wire::common::Identifier& parent, const wire::common::Identifier& partner);

    /// parity: `EnterBlockTrigger`, fired once per tick for every block the player stands in.
    void onEnteredBlock(std::string_view player, std::string_view uuid, const wire::common::Identifier& block);

    /// parity: `RecipeCraftedTrigger`, fired when the result of a recipe is taken out of its slot.
    void onRecipeCrafted(std::string_view player, std::string_view uuid, const wire::common::Identifier& recipe);

    /// parity: `PlayerTrigger` as `location`, which the jar fires once a second: the biome the
    /// player stands in, and the structure whose piece is under them. Either may be absent.
    void onLocation(std::string_view player, std::string_view uuid,
                    const std::optional<wire::common::Identifier>& biome,
                    const std::function<bool(const wire::common::Identifier&)>& insideStructure);

    /// parity: `LootTableTrigger`, a container whose loot this player rolled.
    void onContainerLoot(std::string_view player, std::string_view uuid, const wire::common::Identifier& table);

    /// parity: `ChangeDimensionTrigger`.
    void onChangedDimension(std::string_view player, std::string_view uuid, const wire::common::Identifier& from,
                            const wire::common::Identifier& to);

    /// parity: `ConsumeItemTrigger`, an item eaten or drunk.
    void onConsumedItem(std::string_view player, std::string_view uuid, const wire::common::Identifier& item);

    /// parity: `ItemUsedOnLocationTrigger` as `placed_block`, read off the block that is now there
    /// rather than off the item that put it there.
    void onPlacedBlock(std::string_view player, std::string_view uuid, std::int32_t stateId);

    /// parity: the same trigger as `item_used_on_block`, fired when using the item did something.
    void onItemUsedOnBlock(std::string_view player, std::string_view uuid, std::int32_t stateId,
                           const wire::common::Identifier& item);

    /// parity: `PlayerInteractTrigger`, fired only when the interaction did something and the item
    /// is what did it.
    void onInteractedWithEntity(std::string_view player, std::string_view uuid, const wire::common::Identifier& target,
                                bool targetIsBaby, const wire::common::Identifier& item);

    /// parity: `FishingRodHookedTrigger`, one call per item the rod brought up.
    void onFishingHooked(std::string_view player, std::string_view uuid, const wire::common::Identifier& item);

    /// parity: `FilledBucketTrigger`, the bucket that came back full.
    void onFilledBucket(std::string_view player, std::string_view uuid, const wire::common::Identifier& bucket);

private:
    /// One criterion of one advancement, as the trigger index refers to it.
    struct CriterionRef {
        wire::common::Identifier advancement;
        std::string criterion;
    };

    void grant(std::string_view player, std::string_view uuid, const CriterionRef& candidate);

    void complete(std::string_view player, std::string_view uuid, const wire::common::Identifier& id,
                  const server::world::advancement::Advancement& advancement);

    [[nodiscard]] bool matches(const server::world::advancement::ItemMatcher& matcher,
                               const wire::common::Identifier& item) const;

    /// Whether a block state is one a criterion names, properties included.
    [[nodiscard]] static bool matchesBlock(const server::world::advancement::BlockMatcher& matcher,
                                           std::int32_t stateId);

    const RealAdvancements* advancements_;
    PlayerProgress* progress_;
    Hooks hooks_;
    /// Every `inventory_changed` criterion, indexed by an item that can satisfy its first matcher.
    std::map<wire::common::Identifier, std::vector<CriterionRef>> byItem_;
    /// Every `recipe_unlocked` criterion, by the recipe it waits for.
    std::map<wire::common::Identifier, std::vector<CriterionRef>> byRecipe_;
    /// The criteria of every other trigger, by kind: a handful each, so a walk is enough.
    std::map<server::world::advancement::ConditionKind, std::vector<CriterionRef>> byKind_;

    /// Walks one kind, granting every criterion whose condition `accepts` answers for.
    void fire(std::string_view player, std::string_view uuid, server::world::advancement::ConditionKind kind,
              const std::function<bool(const server::world::advancement::CriterionCondition&)>& accepts);

    /// parity: an entity predicate that names types, ids or one `#tag`; empty means any.
    [[nodiscard]] bool matchesType(const std::vector<std::string>& wanted, const wire::common::Identifier& type) const;
};

} // namespace basalt::server::runtime::world
