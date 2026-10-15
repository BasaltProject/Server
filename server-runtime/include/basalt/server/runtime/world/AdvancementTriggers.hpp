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

struct TriggerItem {
    wire::common::Identifier item;
    std::int32_t count = 1;
    wire::nbt::NbtCompound components;
};

struct TriggerDamage {
    float dealt = 0.0F;
    float taken = 0.0F;
    bool blocked = false;
    wire::common::Identifier type = wire::common::Identifier::vanilla("generic");
    std::optional<server::world::loot::EntityFacts> direct;
    std::optional<server::world::loot::EntityFacts> source;
    bool directHit = false;
};

struct TriggerEvent {
    std::map<std::string, server::world::loot::EntityFacts, std::less<>> entities;
    std::vector<server::world::loot::EntityFacts> victims;
    std::map<std::string, TriggerItem, std::less<>> items;
    std::map<std::string, double, std::less<>> numbers;
    std::map<std::string, wire::common::Identifier, std::less<>> ids;
    std::map<std::string, std::string> state;
    std::optional<std::array<double, 3>> startPosition;
    std::optional<TriggerDamage> damage;
    std::vector<TriggerItem> ingredients;
    std::optional<server::world::loot::LootContext> location;
};

class AdvancementTriggers {
public:
    using ItemTagMembers = std::function<std::set<wire::common::Identifier>(const wire::common::Identifier& tag)>;

    using Announce = std::function<void(std::string_view player, const wire::text::Component& message)>;

    using AwardExperience = std::function<void(std::string_view player, std::int32_t amount)>;

    using AwardRecipe = std::function<void(std::string_view player, const wire::common::Identifier& recipe)>;

    using EntityTagged = std::function<bool(const wire::common::Identifier& type, std::string_view tag)>;

    struct Hooks {
        ItemTagMembers itemTagMembers;
        EntityTagged entityTagged;
        Announce announce;
        AwardExperience awardExperience;
        AwardRecipe awardRecipe;
        server::world::loot::TagLookup tags;
        std::function<bool(const wire::common::Identifier& damageType, std::string_view tag)> damageTagged;
        std::function<std::optional<server::world::loot::LootContext>(std::string_view uuid)> playerContext;
        std::function<void(std::string_view uuid, const std::vector<wire::common::Identifier>& tables)> awardLoot;
        std::function<void(std::string_view uuid, const wire::common::Identifier& function)> runFunction;
        std::function<void(std::string_view uuid, bool showAdvancements)> flush;
    };

    AdvancementTriggers(const RealAdvancements& advancements, PlayerProgress& progress, Hooks hooks = {});

    void reindex();

    bool award(std::string_view player, std::string_view uuid, const wire::common::Identifier& advancement,
               std::string_view criterion);

    void grantAutomatic(std::string_view player, std::string_view uuid);

    void flush(std::string_view uuid, bool showAdvancements) const;

    void fire(std::string_view trigger, std::string_view player, std::string_view uuid, const TriggerEvent& event = {});

    /// parity: `InventoryChangeTrigger`.
    void onInventoryChanged(std::string_view player, std::string_view uuid,
                            const std::vector<std::optional<storage::StoredItemStack>>& inventory);

    void onRecipeUnlocked(std::string_view player, std::string_view uuid, const wire::common::Identifier& recipe);

    void onRecipeCrafted(std::string_view player, std::string_view uuid, const wire::common::Identifier& recipe);

    void onContainerLoot(std::string_view player, std::string_view uuid, const wire::common::Identifier& table);

    void onChangedDimension(std::string_view player, std::string_view uuid, const wire::common::Identifier& from,
                            const wire::common::Identifier& to);

    [[nodiscard]] bool listensTo(std::string_view trigger) const;

private:
    struct CriterionRef {
        wire::common::Identifier advancement;
        std::string criterion;
    };

    void grant(std::string_view player, std::string_view uuid, const CriterionRef& candidate);

    void complete(std::string_view player, std::string_view uuid, const wire::common::Identifier& id,
                  const server::world::advancement::Advancement& advancement);

    void reward(std::string_view player, std::string_view uuid,
                const server::world::advancement::AdvancementRewards& rewards);

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
