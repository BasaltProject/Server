#pragma once

#include <basalt/server/runtime/world/PlayerProgress.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/server/world/loot/LootContext.hpp>
#include <basalt/server/world/loot/LootPredicates.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/text/Component.hpp>

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// One stack a trigger is told about.
struct TriggerItem {
    wire::common::Identifier item;
    std::int32_t count = 1;
    wire::nbt::NbtCompound components;
};

/// A blow a trigger is told about. parity: `DamageSource` and the amounts `hurtServer` works out.
struct TriggerDamage {
    float dealt = 0.0F;
    float taken = 0.0F;
    bool blocked = false;
    wire::common::Identifier type = wire::common::Identifier::vanilla("generic");
    std::optional<server::world::loot::EntityFacts> direct;
    std::optional<server::world::loot::EntityFacts> source;
    /// parity: `DamageSource.isDirect`, whoever caused it also landed it.
    bool directHit = false;
};

/// What one firing of a trigger knows, by the field names its criteria read. parity: the arguments
/// each `SimpleCriterionTrigger.trigger` passes on. The player's own facts come from the hooks.
struct TriggerEvent {
    std::map<std::string, server::world::loot::EntityFacts, std::less<>> entities;
    std::vector<server::world::loot::EntityFacts> victims;
    std::map<std::string, TriggerItem, std::less<>> items;
    std::map<std::string, double, std::less<>> numbers;
    std::map<std::string, wire::common::Identifier, std::less<>> ids;
    std::map<std::string, std::string> state;
    /// Where a fall, a flight or a ride began, for `start_position` and `distance`.
    std::optional<std::array<double, 3>> startPosition;
    std::optional<TriggerDamage> damage;
    std::vector<TriggerItem> ingredients;
    /// The block and tool a `location` predicate runs against.
    std::optional<server::world::loot::LootContext> location;
};

/// What a player just did, and which criteria that satisfies.
///
/// parity: `CriteriaTriggers` plus `PlayerAdvancements.award`. Criteria are indexed once at startup,
/// by item and recipe for the two that fire all the time and by trigger for the rest, whose
/// conditions are read field by field against what the event says.
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
        /// Any registry's tags, which the predicates ask about by registry name.
        server::world::loot::TagLookup tags;
        /// Whether a damage type carries a tag, for `DamageSourcePredicate.tags`.
        std::function<bool(const wire::common::Identifier& damageType, std::string_view tag)> damageTagged;
        /// What a player's own predicates run in: their facts as `this` and their position as the
        /// origin. parity: `EntityPredicate.createContext(player, player)`.
        std::function<std::optional<server::world::loot::LootContext>(std::string_view uuid)> playerContext;
    };

    AdvancementTriggers(const RealAdvancements& advancements, PlayerProgress& progress, Hooks hooks = {});

    /// Fires any trigger read field by field, by its id. parity: `SimpleCriterionTrigger.trigger`.
    void fire(std::string_view trigger, std::string_view player, std::string_view uuid, const TriggerEvent& event = {});

    /// parity: `InventoryChangeTrigger`, which vanilla fires on any inventory change rather than on
    /// the item that changed, so an advancement whose matcher was already satisfied still completes
    /// the first time anything else moves.
    void onInventoryChanged(std::string_view player, std::string_view uuid,
                            const std::vector<std::optional<storage::StoredItemStack>>& inventory);

    /// parity: `RecipeUnlockedTrigger`, fired when a recipe enters the book rather than when it is
    /// crafted.
    void onRecipeUnlocked(std::string_view player, std::string_view uuid, const wire::common::Identifier& recipe);

    void onRecipeCrafted(std::string_view player, std::string_view uuid, const wire::common::Identifier& recipe);

    void onContainerLoot(std::string_view player, std::string_view uuid, const wire::common::Identifier& table);

    void onChangedDimension(std::string_view player, std::string_view uuid, const wire::common::Identifier& from,
                            const wire::common::Identifier& to);

    /// Whether a criterion of this trigger could ever fire, so a caller can skip building facts.
    [[nodiscard]] bool listensTo(std::string_view trigger) const;

private:
    struct CriterionRef {
        wire::common::Identifier advancement;
        std::string criterion;
    };

    void grant(std::string_view player, std::string_view uuid, const CriterionRef& candidate);

    void complete(std::string_view player, std::string_view uuid, const wire::common::Identifier& id,
                  const server::world::advancement::Advancement& advancement);

    [[nodiscard]] bool matches(const server::world::advancement::ItemMatcher& matcher,
                               const wire::common::Identifier& item) const;

    [[nodiscard]] bool accepts(const server::world::advancement::GenericCondition& condition, const TriggerEvent& event,
                               server::world::loot::LootContext& context) const;

    [[nodiscard]] bool accepts(const server::world::advancement::DamageSourceCheck& check, const TriggerDamage& damage,
                               const server::world::loot::LootContext& context) const;

    void fire(std::string_view player, std::string_view uuid, server::world::advancement::ConditionKind kind,
              const std::function<bool(const server::world::advancement::CriterionCondition&)>& accepts);

    const RealAdvancements* advancements_;
    PlayerProgress* progress_;
    Hooks hooks_;
    server::world::loot::TagLookup tags_;
    std::map<wire::common::Identifier, std::vector<CriterionRef>> byItem_;
    std::map<wire::common::Identifier, std::vector<CriterionRef>> byRecipe_;
    std::map<server::world::advancement::ConditionKind, std::vector<CriterionRef>> byKind_;
    std::map<std::string, std::vector<CriterionRef>, std::less<>> byTrigger_;
    mutable std::atomic<std::int64_t> seed_{0};
};

} // namespace basalt::server::runtime::world
