#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/world/loot/LootCondition.hpp>

#include <algorithm>
#include <cmath>
#include <expected>
#include <functional>
#include <map>
#include <utility>

namespace basalt::server::runtime::world {
namespace {

namespace adv = server::world::advancement;
namespace loot = server::world::loot;

using wire::common::Identifier;
using wire::text::Component;

/// parity: `Advancement.name`, the title in square brackets and the type's own colour.
[[nodiscard]] Component nameOf(const Component& title, std::string_view type) {
    const wire::text::TextColor colour = type == "challenge" ? wire::text::kLightPurple : wire::text::kGreen;
    return Component::text("[").append(title).append(Component::text("]")).color(colour);
}

/// Criteria written before a namespace was required name their trigger bare.
[[nodiscard]] std::string triggerKey(std::string_view trigger) {
    return trigger.find(':') == std::string_view::npos ? "minecraft:" + std::string{trigger} : std::string{trigger};
}

[[nodiscard]] bool allPass(const std::vector<loot::LootCondition>& conditions, random::RandomSource& random,
                           const loot::LootContext& context) {
    return std::ranges::all_of(
        conditions, [&random, &context](const loot::LootCondition& one) { return one.test(random, context); });
}

[[nodiscard]] bool itemPasses(const loot::ItemPredicate& predicate, const TriggerItem& item,
                              const loot::TagLookup& tags) {
    return predicate.test(item.item, item.components, tags, item.count);
}

} // namespace

AdvancementTriggers::AdvancementTriggers(const RealAdvancements& advancements, PlayerProgress& progress, Hooks hooks)
    : advancements_(&advancements), progress_(&progress), hooks_(std::move(hooks)) {
    tags_ = hooks_.tags;
    if (!tags_) {
        tags_ = [this](std::string_view registry, const Identifier& id, std::string_view tag) {
            const std::expected<Identifier, wire::common::IdentifierError> parsed = Identifier::parse(tag);
            if (registry == "item") {
                return parsed.has_value() && hooks_.itemTagMembers && hooks_.itemTagMembers(*parsed).contains(id);
            }
            return registry == "entity_type" && hooks_.entityTagged && hooks_.entityTagged(id, tag);
        };
    }
    reindex();
}

void AdvancementTriggers::reindex() {
    byItem_.clear();
    byRecipe_.clear();
    byKind_.clear();
    byTrigger_.clear();
    for (const Identifier& id : advancements_->ids()) {
        const adv::Advancement* advancement = advancements_->find(id);
        if (advancement == nullptr) {
            continue;
        }
        for (const auto& [name, criterion] : advancement->criteria) {
            if (criterion.condition.kind == adv::ConditionKind::InventoryItems) {
                // Indexed by every item that can satisfy the *first* matcher: the rest are checked
                // when the candidate comes up, and one entry per matcher would repeat the work.
                if (criterion.condition.matchers.empty()) {
                    continue;
                }
                const adv::ItemMatcher& first = criterion.condition.matchers.front();
                std::set<Identifier> items{first.items.begin(), first.items.end()};
                if (first.tag.has_value() && hooks_.itemTagMembers) {
                    const std::set<Identifier> tagged = hooks_.itemTagMembers(*first.tag);
                    items.insert(tagged.begin(), tagged.end());
                }
                for (const Identifier& item : items) {
                    byItem_[item].push_back(CriterionRef{.advancement = id, .criterion = name});
                }
            } else if (criterion.condition.kind == adv::ConditionKind::RecipeUnlocked &&
                       criterion.condition.recipe.has_value()) {
                byRecipe_[*criterion.condition.recipe].push_back(CriterionRef{.advancement = id, .criterion = name});
            } else if (criterion.condition.kind == adv::ConditionKind::Generic) {
                byTrigger_[criterion.trigger.toString()].push_back(CriterionRef{.advancement = id, .criterion = name});
            } else if (criterion.condition.kind != adv::ConditionKind::Unsupported) {
                byKind_[criterion.condition.kind].push_back(CriterionRef{.advancement = id, .criterion = name});
            }
        }
    }
}

bool AdvancementTriggers::matches(const adv::ItemMatcher& matcher, const Identifier& item) const {
    if (std::ranges::find(matcher.items, item) != matcher.items.end()) {
        return true;
    }
    if (!matcher.tag.has_value() || !hooks_.itemTagMembers) {
        return false;
    }
    return hooks_.itemTagMembers(*matcher.tag).contains(item);
}

void AdvancementTriggers::onInventoryChanged(std::string_view player, std::string_view uuid,
                                             const std::vector<std::optional<storage::StoredItemStack>>& inventory) {
    std::set<Identifier> items;
    for (const std::optional<storage::StoredItemStack>& stack : inventory) {
        if (!stack.has_value()) {
            continue;
        }
        const std::expected<Identifier, wire::common::IdentifierError> parsed = Identifier::parse(stack->id);
        if (parsed.has_value()) {
            items.insert(*parsed);
        }
    }
    if (items.empty()) {
        return;
    }
    std::vector<CriterionRef> candidates;
    for (const Identifier& item : items) {
        const auto found = byItem_.find(item);
        if (found == byItem_.end()) {
            continue;
        }
        for (const CriterionRef& candidate : found->second) {
            const bool seen = std::ranges::any_of(candidates, [&candidate](const CriterionRef& other) {
                return other.advancement == candidate.advancement && other.criterion == candidate.criterion;
            });
            if (!seen) {
                candidates.push_back(candidate);
            }
        }
    }
    for (const CriterionRef& candidate : candidates) {
        const adv::Advancement* advancement = advancements_->find(candidate.advancement);
        if (advancement == nullptr) {
            continue;
        }
        const adv::AdvancementCriterion* criterion = advancement->criterion(candidate.criterion);
        if (criterion == nullptr || criterion->condition.kind != adv::ConditionKind::InventoryItems) {
            continue;
        }
        const bool satisfied =
            std::ranges::all_of(criterion->condition.matchers, [this, &items](const adv::ItemMatcher& matcher) {
                return std::ranges::any_of(items,
                                           [this, &matcher](const Identifier& item) { return matches(matcher, item); });
            });
        if (satisfied) {
            grant(player, uuid, candidate);
        }
    }
}

void AdvancementTriggers::onRecipeUnlocked(std::string_view player, std::string_view uuid, const Identifier& recipe) {
    const auto found = byRecipe_.find(recipe);
    if (found == byRecipe_.end()) {
        return;
    }
    for (const CriterionRef& candidate : found->second) {
        grant(player, uuid, candidate);
    }
}

void AdvancementTriggers::fire(std::string_view player, std::string_view uuid, adv::ConditionKind kind,
                               const std::function<bool(const adv::CriterionCondition&)>& accepts) {
    const auto found = byKind_.find(kind);
    if (found == byKind_.end()) {
        return;
    }
    for (const CriterionRef& candidate : found->second) {
        const adv::Advancement* advancement = advancements_->find(candidate.advancement);
        const adv::AdvancementCriterion* criterion =
            advancement == nullptr ? nullptr : advancement->criterion(candidate.criterion);
        if (criterion != nullptr && accepts(criterion->condition)) {
            grant(player, uuid, candidate);
        }
    }
}

void AdvancementTriggers::onRecipeCrafted(std::string_view player, std::string_view uuid, const Identifier& recipe) {
    fire(player, uuid, adv::ConditionKind::RecipeCrafted, [&recipe](const adv::CriterionCondition& condition) {
        return condition.recipe.has_value() && *condition.recipe == recipe;
    });
}

void AdvancementTriggers::onContainerLoot(std::string_view player, std::string_view uuid, const Identifier& table) {
    fire(player, uuid, adv::ConditionKind::ContainerLoot, [&table](const adv::CriterionCondition& condition) {
        return condition.lootTable.has_value() && *condition.lootTable == table;
    });
}

void AdvancementTriggers::onChangedDimension(std::string_view player, std::string_view uuid, const Identifier& from,
                                             const Identifier& to) {
    fire(player, uuid, adv::ConditionKind::ChangedDimension, [&from, &to](const adv::CriterionCondition& condition) {
        return (!condition.fromDimension.has_value() || *condition.fromDimension == from) &&
               (!condition.toDimension.has_value() || *condition.toDimension == to);
    });
}

bool AdvancementTriggers::listensTo(std::string_view trigger) const { return byTrigger_.contains(triggerKey(trigger)); }

void AdvancementTriggers::fire(std::string_view trigger, std::string_view player, std::string_view uuid,
                               const TriggerEvent& event) {
    const auto found = byTrigger_.find(triggerKey(trigger));
    if (found == byTrigger_.end()) {
        return;
    }
    // Only what is still open is worth building the player's context for; most fire every tick.
    std::vector<std::pair<const CriterionRef*, const adv::GenericCondition*>> open;
    for (const CriterionRef& candidate : found->second) {
        const adv::Advancement* advancement = advancements_->find(candidate.advancement);
        const adv::AdvancementCriterion* criterion =
            advancement == nullptr ? nullptr : advancement->criterion(candidate.criterion);
        if (criterion == nullptr || criterion->condition.generic == nullptr ||
            progress_->grantedCriteria(uuid, candidate.advancement).contains(candidate.criterion)) {
            continue;
        }
        open.emplace_back(&candidate, criterion->condition.generic.get());
    }
    if (open.empty()) {
        return;
    }
    std::optional<loot::LootContext> built = hooks_.playerContext ? hooks_.playerContext(uuid) : std::nullopt;
    loot::LootContext context = built.has_value() ? *std::move(built) : loot::LootContext{};
    if (!context.thisEntity.has_value()) {
        context.thisEntity = loot::EntityFacts{.type = Identifier::vanilla("player"), .scoreName = std::string{player}};
    }
    context.tags = tags_;
    context.hasEntity = true;
    const bool consumesVictims = triggerKey(trigger) == "minecraft:killed_by_arrow";
    for (const auto& [candidate, condition] : open) {
        if (!accepts(*condition, event, context)) {
            continue;
        }
        if (!consumesVictims || condition->victims.empty()) {
            grant(player, uuid, *candidate);
            continue;
        }
        // parity: `KilledByArrowTrigger`, where each predicate takes a victim of its own.
        std::vector<const loot::EntityFacts*> left;
        for (const loot::EntityFacts& victim : event.victims) {
            left.push_back(&victim);
        }
        random::RandomSource random = random::RandomSource::legacy(seed_.fetch_add(1));
        loot::LootContext victimContext = context;
        const bool all = std::ranges::all_of(condition->victims, [&](const std::vector<loot::LootCondition>& one) {
            for (auto at = left.begin(); at != left.end(); ++at) {
                victimContext.thisEntity = **at;
                if (allPass(one, random, victimContext)) {
                    left.erase(at);
                    return true;
                }
            }
            return false;
        });
        if (all) {
            grant(player, uuid, *candidate);
        }
    }
}

bool AdvancementTriggers::accepts(const adv::DamageSourceCheck& check, const TriggerDamage& damage,
                                  const loot::LootContext& context) const {
    for (const auto& [tag, expected] : check.tags) {
        const std::string_view name = std::string_view{tag}.starts_with('#') ? std::string_view{tag}.substr(1) : tag;
        const bool carries = hooks_.damageTagged && hooks_.damageTagged(damage.type, name);
        if (carries != expected) {
            return false;
        }
    }
    const std::optional<std::array<double, 3>> origin =
        context.thisEntity.has_value() ? context.thisEntity->position : std::nullopt;
    if (check.direct != nullptr && !check.direct->test(damage.direct, tags_, origin)) {
        return false;
    }
    if (check.source != nullptr && !check.source->test(damage.source, tags_, origin)) {
        return false;
    }
    return !check.isDirect.has_value() || *check.isDirect == damage.directHit;
}

bool AdvancementTriggers::accepts(const adv::GenericCondition& condition, const TriggerEvent& event,
                                  loot::LootContext& context) const {
    const loot::EntityFacts player = *context.thisEntity;
    const std::optional<std::array<double, 3>> here = player.position;
    random::RandomSource random = random::RandomSource::legacy(seed_.fetch_add(1));
    const auto entityPasses = [&](const std::vector<loot::LootCondition>& conditions, const loot::EntityFacts& entity) {
        context.thisEntity = entity;
        if (!context.thisEntity->placeAt) {
            context.thisEntity->placeAt = context.placeAt;
        }
        const bool passed = allPass(conditions, random, context);
        context.thisEntity = player;
        return passed;
    };
    for (const auto& [field, conditions] : condition.entities) {
        if (field == "player") {
            if (!allPass(conditions, random, context)) {
                return false;
            }
            continue;
        }
        const auto entity = event.entities.find(field);
        if (entity == event.entities.end() || !entityPasses(conditions, entity->second)) {
            return false;
        }
    }
    // parity: `ChanneledLightningTrigger`, each predicate met by any victim.
    for (const std::vector<loot::LootCondition>& victim : condition.victims) {
        if (!std::ranges::any_of(event.victims,
                                 [&](const loot::EntityFacts& one) { return entityPasses(victim, one); })) {
            return false;
        }
    }
    for (const auto& [field, predicate] : condition.items) {
        const auto item = event.items.find(field);
        if (item == event.items.end() || !itemPasses(predicate, item->second, tags_)) {
            return false;
        }
    }
    for (const auto& [field, bounds] : condition.numbers) {
        const auto number = event.numbers.find(field);
        if (number == event.numbers.end() || !bounds.test(number->second)) {
            return false;
        }
    }
    for (const auto& [field, wanted] : condition.ids) {
        const auto id = event.ids.find(field);
        const std::string_view registry = field == "potion"                            ? "potion"
                                          : field == "recipes" || field == "recipe_id" ? "recipe"
                                                                                       : "block";
        if (id == event.ids.end() || !loot::matchesIdsOrTag(wanted, registry, id->second, tags_)) {
            return false;
        }
    }
    if (condition.state.has_value() && !condition.state->test(event.state)) {
        return false;
    }
    if (condition.distance.has_value() && (!event.startPosition.has_value() || !here.has_value() ||
                                           !condition.distance->test(*event.startPosition, *here))) {
        return false;
    }
    if (condition.startPosition != nullptr) {
        if (!event.startPosition.has_value() || !context.placeAt) {
            return false;
        }
        const std::array<double, 3>& start = *event.startPosition;
        loot::PlaceFacts place = context.placeAt(static_cast<std::int32_t>(std::floor(start[0])),
                                                 static_cast<std::int32_t>(std::floor(start[1])),
                                                 static_cast<std::int32_t>(std::floor(start[2])));
        place.exact = start;
        if (!condition.startPosition->test(place, tags_)) {
            return false;
        }
    }
    if (!condition.effects.empty() && !loot::EntityPredicate{.effects = condition.effects}.test(player, tags_, here)) {
        return false;
    }
    if (condition.damage.has_value()) {
        const adv::DamageCheck& check = *condition.damage;
        if (!event.damage.has_value()) {
            return false;
        }
        const TriggerDamage& damage = *event.damage;
        if (!check.dealt.test(damage.dealt) || !check.taken.test(damage.taken) ||
            (check.blocked.has_value() && *check.blocked != damage.blocked) ||
            (check.source != nullptr && !check.source->test(damage.source, tags_, here)) ||
            (check.type.has_value() && !accepts(*check.type, damage, context))) {
            return false;
        }
    }
    if (condition.killingBlow.has_value() &&
        (!event.damage.has_value() || !accepts(*condition.killingBlow, *event.damage, context))) {
        return false;
    }
    // parity: `RecipeCraftedTrigger`, each predicate taking an ingredient of its own.
    if (!condition.ingredients.empty()) {
        std::vector<const TriggerItem*> left;
        for (const TriggerItem& ingredient : event.ingredients) {
            left.push_back(&ingredient);
        }
        for (const loot::ItemPredicate& wanted : condition.ingredients) {
            const auto match = std::ranges::find_if(
                left, [this, &wanted](const TriggerItem* one) { return itemPasses(wanted, *one, tags_); });
            if (match == left.end()) {
                return false;
            }
            left.erase(match);
        }
    }
    if (condition.location.has_value()) {
        if (!event.location.has_value()) {
            return false;
        }
        loot::LootContext location = *event.location;
        location.tags = tags_;
        if (!allPass(*condition.location, random, location)) {
            return false;
        }
    }
    return true;
}

void AdvancementTriggers::grant(std::string_view player, std::string_view uuid, const CriterionRef& candidate) {
    const adv::Advancement* advancement = advancements_->find(candidate.advancement);
    if (advancement == nullptr) {
        return;
    }
    const bool wasDone = advancement->isComplete(progress_->grantedCriteria(uuid, candidate.advancement));
    if (!progress_->grantCriterion(uuid, candidate.advancement, candidate.criterion) || wasDone) {
        return;
    }
    if (!advancement->isComplete(progress_->grantedCriteria(uuid, candidate.advancement))) {
        return;
    }
    complete(player, uuid, candidate.advancement, *advancement);
}

void AdvancementTriggers::complete(std::string_view player, std::string_view uuid, const Identifier& id,
                                   const adv::Advancement& advancement) {
    static_cast<void>(id);
    if (advancement.rewards.experience > 0 && hooks_.awardExperience) {
        hooks_.awardExperience(player, advancement.rewards.experience);
    }
    for (const Identifier& recipe : advancement.rewards.recipes) {
        if (progress_->learn(uuid, recipe) && hooks_.awardRecipe) {
            hooks_.awardRecipe(player, recipe);
        }
    }
    if (!advancement.display.has_value() || !advancement.display->announceToChat || !hooks_.announce) {
        return;
    }
    hooks_.announce(player, Component::translatable("chat.type.advancement." + advancement.display->type,
                                                    {Component::text(std::string{player}),
                                                     nameOf(advancement.display->title, advancement.display->type)}));
}

} // namespace basalt::server::runtime::world
