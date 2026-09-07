#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/RegistryArguments.hpp>
#include <basalt/server/runtime/command/commands/ProgressCommands.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/RecipeBook.hpp>

#include <algorithm>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace basalt::server::runtime::command::commands {
namespace {

using wire::common::Identifier;
using wire::text::Component;

[[nodiscard]] const std::vector<player::PlayerSession*>& targetsOf(const CommandContext& context) {
    static const std::vector<player::PlayerSession*> none;
    const auto* found = context.get<std::vector<player::PlayerSession*>>("targets");
    return found == nullptr ? none : *found;
}

} // namespace

AdvancementCommand::AdvancementCommand(const world::RealAdvancements& advancements, world::PlayerProgress& progress)
    : Command("advancement", permissionLevel::kGamemasters), advancements_(&advancements), progress_(&progress) {}

void AdvancementCommand::build(const CommandNodeBuilder& root) const {
    /// parity: the one report every form shares, whose key names how many of each side there were.
    const auto report = [](const CommandContext& context, bool granting, std::int32_t changed,
                           const std::vector<player::PlayerSession*>& targets,
                           bool one) -> std::optional<CommandSyntaxError> {
        const std::string verb = granting ? "grant" : "revoke";
        const std::string which = one ? "one" : "many";
        if (changed == 0) {
            return CommandSyntaxError{
                .component = Component::translatable(
                    "commands.advancement." + verb + "." + which + ".to.one.failure",
                    {Component::text(targets.empty() ? std::string{} : targets.front()->name())})};
        }
        context.success(
            Component::translatable(
                "commands.advancement." + verb + "." + which + ".to." + (targets.size() == 1 ? "one" : "many") +
                    ".success",
                {Component::text(std::to_string(changed)),
                 Component::text(targets.size() == 1 ? targets.front()->name() : std::to_string(targets.size()))}),
            true);
        return std::nullopt;
    };

    const auto apply = [this, report](const CommandContext& context, bool granting,
                                      const std::vector<Identifier>& ids) -> Parsed<std::int32_t> {
        const std::vector<player::PlayerSession*>& targets = targetsOf(context);
        std::int32_t changed = 0;
        for (const player::PlayerSession* target : targets) {
            for (const Identifier& id : ids) {
                const server::world::advancement::Advancement* advancement = advancements_->find(id);
                if (advancement == nullptr) {
                    continue;
                }
                if (granting) {
                    for (const auto& [criterion, definition] : advancement->criteria) {
                        if (progress_->grantCriterion(target->uuid().toString(), id, criterion)) {
                            changed++;
                        }
                    }
                } else if (progress_->revokeAll(target->uuid().toString(), id)) {
                    changed++;
                }
            }
        }
        if (const std::optional<CommandSyntaxError> failure =
                report(context, granting, changed, targets, ids.size() == 1);
            failure.has_value()) {
            return std::unexpected{*failure};
        }
        return changed;
    };

    const auto applyCriterion = [this, report](const CommandContext& context, bool granting,
                                               const Identifier& id) -> Parsed<std::int32_t> {
        const server::world::advancement::Advancement* advancement = advancements_->find(id);
        const auto* criterion = context.get<std::string>("criterion");
        if (advancement == nullptr || criterion == nullptr) {
            return 0;
        }
        if (advancement->criterion(*criterion) == nullptr) {
            return std::unexpected{CommandSyntaxError{
                .component = Component::translatable("commands.advancement.criterionNotFound",
                                                     {Component::text(id.toString()), Component::text(*criterion)})}};
        }
        const std::vector<player::PlayerSession*>& targets = targetsOf(context);
        std::int32_t changed = 0;
        for (const player::PlayerSession* target : targets) {
            const bool moved = granting ? progress_->grantCriterion(target->uuid().toString(), id, *criterion)
                                        : progress_->revokeCriterion(target->uuid().toString(), id, *criterion);
            if (moved) {
                changed++;
            }
        }
        if (const std::optional<CommandSyntaxError> failure = report(context, granting, changed, targets, true);
            failure.has_value()) {
            return std::unexpected{*failure};
        }
        return changed;
    };

    const auto selection = [this, apply, applyCriterion](const CommandContext& context, bool granting,
                                                         std::string_view mode, bool named) -> Parsed<std::int32_t> {
        const auto* id = context.get<Identifier>("advancement");
        if (id == nullptr) {
            return 0;
        }
        if (advancements_->find(*id) == nullptr) {
            return std::unexpected{
                CommandSyntaxError{.component = Component::translatable("advancement.advancementNotFound",
                                                                        {Component::text(id->toString())})}};
        }
        if (named) {
            return applyCriterion(context, granting, *id);
        }
        std::vector<Identifier> selected;
        if (mode == "only") {
            selected.push_back(*id);
        } else if (mode == "from") {
            selected = advancements_->descendantsOf(*id);
        } else if (mode == "until") {
            selected = advancements_->ancestorsOf(*id);
        } else {
            selected = advancements_->ancestorsOf(*id);
            for (const Identifier& one : advancements_->descendantsOf(*id)) {
                if (std::ranges::find(selected, one) == selected.end()) {
                    selected.push_back(one);
                }
            }
        }
        return apply(context, granting, selected);
    };

    for (const bool granting : {true, false}) {
        root.literal(granting ? "grant" : "revoke", [this, granting, apply, selection](const CommandNodeBuilder& form) {
            form.argument(
                "targets", playersArgument(false),
                [this, granting, apply, selection](const CommandNodeBuilder& targets) {
                    targets.literal("everything", [this, granting, apply](const CommandNodeBuilder& everything) {
                        everything.executes([this, granting, apply](const CommandContext& context) {
                            return apply(context, granting, advancements_->ids());
                        });
                    });
                    for (const std::string_view mode : {"only", "from", "through", "until"}) {
                        targets.literal(std::string{mode}, [this, granting, mode,
                                                            selection](const CommandNodeBuilder& form) {
                            form.argument(
                                "advancement", resourceKeyArgument("minecraft:advancement"),
                                [this, granting, mode, selection](const CommandNodeBuilder& named) {
                                    named.suggestsFromServer([this](const SuggestionContext& context) {
                                        return context.matchingIds(advancements_->ids());
                                    });
                                    named.executes([granting, mode, selection](const CommandContext& context) {
                                        return selection(context, granting, mode, false);
                                    });
                                    if (mode != "only") {
                                        return;
                                    }
                                    named.argument(
                                        "criterion", wordArgument(),
                                        [this, granting, mode, selection](const CommandNodeBuilder& one) {
                                            /// Only the criteria of the advancement already
                                            /// named, which is what vanilla completes here.
                                            one.suggestsFromServer([this](const SuggestionContext& context) {
                                                std::vector<std::string> named;
                                                const auto* id = context.argument<Identifier>("advancement");
                                                const server::world::advancement::Advancement* advancement =
                                                    id == nullptr ? nullptr : advancements_->find(*id);
                                                if (advancement != nullptr) {
                                                    for (const auto& [criterion, definition] : advancement->criteria) {
                                                        named.push_back(criterion);
                                                    }
                                                }
                                                return context.matching(named);
                                            });
                                            one.executes([granting, mode, selection](const CommandContext& context) {
                                                return selection(context, granting, mode, true);
                                            });
                                        });
                                });
                        });
                    }
                });
        });
    }
}

RecipeCommand::RecipeCommand(world::PlayerProgress& progress, const world::RealRecipes& recipes)
    : Command("recipe", permissionLevel::kGamemasters), progress_(&progress), recipes_(&recipes) {}

void RecipeCommand::build(const CommandNodeBuilder& root) const {
    const auto apply = [this](const CommandContext& context, bool giving,
                              const Identifier* recipe) -> Parsed<std::int32_t> {
        const std::set<Identifier>& known = recipes_->ids();
        if (recipe != nullptr && !known.empty() && !known.contains(*recipe)) {
            return std::unexpected{CommandSyntaxError{
                .component = Component::translatable("recipe.notFound", {Component::text(recipe->toString())})}};
        }
        const std::vector<Identifier> ids =
            recipe != nullptr ? std::vector<Identifier>{*recipe} : std::vector<Identifier>{known.begin(), known.end()};
        const std::vector<player::PlayerSession*>& targets = targetsOf(context);
        std::int32_t changed = 0;
        std::vector<Identifier> moved;
        for (const player::PlayerSession* target : targets) {
            for (const Identifier& id : ids) {
                bool applied = false;
                if (giving) {
                    applied = progress_->learn(target->uuid().toString(), id);
                    // parity: `ServerPlayer.awardRecipes` fires the unlock trigger itself, rather
                    // than leaving the advancement to whatever crafting follows.
                    if (applied && context.environment().options().advancementTriggers != nullptr) {
                        context.environment().options().advancementTriggers->onRecipeUnlocked(
                            target->name(), target->uuid().toString(), id);
                    }
                } else {
                    applied = progress_->forget(target->uuid().toString(), id);
                }
                if (applied) {
                    changed++;
                    if (std::ranges::find(moved, id) == moved.end()) {
                        moved.push_back(id);
                    }
                }
            }
            if (!moved.empty()) {
                // Adds or removes the entries in this player's own book.
                if (giving) {
                    world::recipeBook::add(*target, *recipes_, moved);
                } else {
                    world::recipeBook::remove(*target, *recipes_, moved);
                }
            }
        }
        const std::string verb = giving ? "give" : "take";
        if (changed == 0) {
            return std::unexpected{
                CommandSyntaxError{.component = Component::translatable("commands.recipe." + verb + ".failed")}};
        }
        context.success(targets.size() == 1
                            ? Component::translatable(
                                  "commands.recipe." + verb + ".success.single",
                                  {Component::text(std::to_string(changed)), Component::text(targets.front()->name())})
                            : Component::translatable("commands.recipe." + verb + ".success.multiple",
                                                      {Component::text(std::to_string(changed)),
                                                       Component::text(std::to_string(targets.size()))}),
                        true);
        return changed;
    };

    for (const bool giving : {true, false}) {
        root.literal(giving ? "give" : "take", [this, giving, apply](const CommandNodeBuilder& form) {
            form.argument("targets", playersArgument(false), [this, giving, apply](const CommandNodeBuilder& targets) {
                targets.literal("*", [giving, apply](const CommandNodeBuilder& all) {
                    all.executes(
                        [giving, apply](const CommandContext& context) { return apply(context, giving, nullptr); });
                });
                targets.argument("recipe", resourceKeyArgument("minecraft:recipe"),
                                 [this, giving, apply](const CommandNodeBuilder& named) {
                                     named.suggestsFromServer([this](const SuggestionContext& context) {
                                         return context.matchingIds(
                                             std::vector<Identifier>{recipes_->ids().begin(), recipes_->ids().end()});
                                     });
                                     named.executes([giving, apply](const CommandContext& context) {
                                         return apply(context, giving, context.get<Identifier>("recipe"));
                                     });
                                 });
            });
        });
    }
}

} // namespace basalt::server::runtime::command::commands
