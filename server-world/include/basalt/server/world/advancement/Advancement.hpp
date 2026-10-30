#pragma once

#include <basalt/server/world/loot/LootCondition.hpp>
#include <basalt/server/world/loot/LootPredicates.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/json/Json.hpp>
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

struct ItemMatcher {
    std::vector<wire::common::Identifier> items;
    std::optional<wire::common::Identifier> tag;
};

/// parity: `CriterionTriggerInstance`.
enum class ConditionKind : std::uint8_t {
    InventoryItems,
    RecipeUnlocked,
    /// parity: `LootTableTrigger` as `player_generates_container_loot`.
    ContainerLoot,
    /// parity: `ChangeDimensionTrigger`.
    ChangedDimension,
    /// parity: `RecipeCraftedTrigger`, the recipe behind a result the player took.
    RecipeCrafted,
    Generic,
    Unsupported,
};

/// parity: `DamageSourcePredicate`.
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

struct GenericCondition {
    std::map<std::string, std::vector<loot::LootCondition>, std::less<>> entities;
    /// parity: `KilledByArrowTrigger` and `ChanneledLightningTrigger`.
    std::vector<std::vector<loot::LootCondition>> victims;
    std::map<std::string, loot::ItemPredicate, std::less<>> items;
    std::map<std::string, loot::DoubleBounds, std::less<>> numbers;
    std::map<std::string, std::vector<std::string>, std::less<>> ids;
    std::optional<loot::StateMatcher> state;
    std::optional<loot::DistancePredicate> distance;
    std::shared_ptr<const loot::LocationPredicate> startPosition;
    std::vector<loot::EffectRequirement> effects;
    std::optional<DamageCheck> damage;
    std::optional<DamageSourceCheck> killingBlow;
    /// parity: `RecipeCraftedTrigger.ingredients`, each predicate a different ingredient.
    std::vector<loot::ItemPredicate> ingredients;
    std::optional<std::vector<loot::LootCondition>> location;
};

struct CriterionCondition {
    ConditionKind kind = ConditionKind::Unsupported;
    std::vector<ItemMatcher> matchers;
    std::optional<wire::common::Identifier> recipe;
    std::optional<wire::common::Identifier> lootTable;
    std::optional<wire::common::Identifier> fromDimension;
    std::optional<wire::common::Identifier> toDimension;
    std::shared_ptr<const GenericCondition> generic;
};

struct AdvancementCriterion {
    wire::common::Identifier trigger;
    CriterionCondition condition;
};

/// AND-of-OR criterion groups. parity: `AdvancementRequirements.test`, every group needs one criterion.
struct AdvancementRequirements {
    std::vector<std::vector<std::string>> groups;

    [[nodiscard]] bool isComplete(const std::set<std::string, std::less<>>& granted) const;
};

struct AdvancementRewards {
    std::int32_t experience = 0;
    std::vector<wire::common::Identifier> loot;
    std::vector<wire::common::Identifier> recipes;
    /// parity: `AdvancementRewards.function`, run as the player when it is earned.
    std::optional<wire::common::Identifier> function;
};

/// The item an advancement's frame shows. parity: `ItemStackTemplate`.
struct AdvancementIcon {
    wire::common::Identifier item = wire::common::Identifier::vanilla("air");
    std::int32_t count = 1;
    std::optional<wire::common::JsonValue> components;
};

/// How an advancement is drawn and announced. parity: `DisplayInfo` plus `AdvancementType`.
struct AdvancementDisplay {
    wire::text::Component title;
    wire::text::Component description;
    std::string type = "task";
    bool announceToChat = true;
    AdvancementIcon icon;
    std::optional<std::string> background;
    bool showToast = true;
    bool hidden = false;
};

/// One advancement definition. parity: vanilla's `Advancement` record.
struct Advancement {
    std::optional<wire::common::Identifier> parent;
    std::vector<std::pair<std::string, AdvancementCriterion>> criteria;
    AdvancementRequirements requirements;
    AdvancementRewards rewards;
    std::optional<AdvancementDisplay> display;
    bool sendsTelemetryEvent = false;

    [[nodiscard]] const AdvancementCriterion* criterion(std::string_view name) const;

    [[nodiscard]] bool isComplete(const std::set<std::string, std::less<>>& granted) const {
        return requirements.isComplete(granted);
    }
};

} // namespace basalt::server::world::advancement
