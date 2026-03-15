#include <basalt/server/runtime/world/AdvancementTriggers.hpp>

#include <algorithm>
#include <expected>
#include <functional>
#include <utility>

namespace basalt::server::runtime::world {
namespace {

namespace adv = server::world::advancement;

using wire::common::Identifier;
using wire::text::Component;

/// parity: `Advancement.name`, the title in square brackets and the type's own colour.
[[nodiscard]] Component nameOf(const Component& title, std::string_view type) {
    const wire::text::TextColor colour = type == "challenge" ? wire::text::kLightPurple : wire::text::kGreen;
    return Component::text("[").append(title).append(Component::text("]")).color(colour);
}

} // namespace

AdvancementTriggers::AdvancementTriggers(const RealAdvancements& advancements, PlayerProgress& progress, Hooks hooks)
    : advancements_(&advancements), progress_(&progress), hooks_(std::move(hooks)) {
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

bool AdvancementTriggers::matchesType(const std::vector<std::string>& wanted, const Identifier& type) const {
    if (wanted.empty()) {
        return true;
    }
    return std::ranges::any_of(wanted, [this, &type](const std::string& name) {
        if (name.starts_with('#')) {
            return hooks_.entityTagged && hooks_.entityTagged(type, std::string_view{name}.substr(1));
        }
        const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(name);
        return id.has_value() && *id == type;
    });
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

void AdvancementTriggers::onKilledEntity(std::string_view player, std::string_view uuid, const Identifier& victim) {
    fire(player, uuid, adv::ConditionKind::KilledEntity, [this, &victim](const adv::CriterionCondition& condition) {
        return matchesType(condition.entityTypes, victim);
    });
}

void AdvancementTriggers::onTamedAnimal(std::string_view player, std::string_view uuid, const Identifier& animal) {
    fire(player, uuid, adv::ConditionKind::TamedAnimal, [this, &animal](const adv::CriterionCondition& condition) {
        return matchesType(condition.entityTypes, animal);
    });
}

void AdvancementTriggers::onSummonedEntity(std::string_view player, std::string_view uuid, const Identifier& summoned) {
    fire(player, uuid, adv::ConditionKind::SummonedEntity, [this, &summoned](const adv::CriterionCondition& condition) {
        return matchesType(condition.entityTypes, summoned);
    });
}

void AdvancementTriggers::onBredAnimals(std::string_view player, std::string_view uuid) {
    fire(player, uuid, adv::ConditionKind::BredAnimals, [](const adv::CriterionCondition&) { return true; });
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

void AdvancementTriggers::onConsumedItem(std::string_view player, std::string_view uuid, const Identifier& item) {
    fire(player, uuid, adv::ConditionKind::ConsumedItem, [this, &item](const adv::CriterionCondition& condition) {
        return condition.matchers.empty() || matches(condition.matchers.front(), item);
    });
}

void AdvancementTriggers::onFilledBucket(std::string_view player, std::string_view uuid, const Identifier& bucket) {
    fire(player, uuid, adv::ConditionKind::FilledBucket, [this, &bucket](const adv::CriterionCondition& condition) {
        return condition.matchers.empty() || matches(condition.matchers.front(), bucket);
    });
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
