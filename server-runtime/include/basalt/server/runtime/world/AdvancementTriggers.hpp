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

    struct Hooks {
        ItemTagMembers itemTagMembers;
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

    const RealAdvancements* advancements_;
    PlayerProgress* progress_;
    Hooks hooks_;
    /// Every `inventory_changed` criterion, indexed by an item that can satisfy its first matcher.
    std::map<wire::common::Identifier, std::vector<CriterionRef>> byItem_;
    /// Every `recipe_unlocked` criterion, by the recipe it waits for.
    std::map<wire::common::Identifier, std::vector<CriterionRef>> byRecipe_;
};

} // namespace basalt::server::runtime::world
