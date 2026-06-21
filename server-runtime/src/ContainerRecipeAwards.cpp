#include <basalt/server/plugins/PluginHookEventTypes.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/ContainerMenuController.hpp>
#include <basalt/server/runtime/plugins/EventSink.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/PlayerProgress.hpp>
#include <basalt/server/runtime/world/RealRecipes.hpp>
#include <basalt/server/runtime/world/RecipeBook.hpp>

#include <set>
#include <vector>

namespace basalt::server::runtime {

void ContainerMenuController::awardRecipes(player::PlayerSession& session,
                                           const std::vector<wire::common::Identifier>& ids) {
    world::PlayerProgress* progress = owner_->options().progress;
    world::RealRecipes* recipes = owner_->options().recipes;
    if (progress == nullptr || recipes == nullptr) {
        return;
    }
    world::AdvancementTriggers* triggers = owner_->options().advancementTriggers;
    const std::string uuid = session.uuid().toString();
    std::vector<wire::common::Identifier> learnt;
    const plugins::EventSink* events = owner_->options().events;
    const bool listened =
        events != nullptr && events->listening(&server::plugins::pluginEventTypes::playerRecipeDiscover);
    const std::set<wire::common::Identifier> known =
        listened ? progress->knownRecipes(uuid) : std::set<wire::common::Identifier>{};
    for (const wire::common::Identifier& id : ids) {
        const server::world::crafting::Recipe* recipe = recipes->recipeFor(id);
        // parity: `Recipe.isSpecial`.
        if (recipe == nullptr || recipe->kind == server::world::crafting::RecipeKind::Special ||
            recipe->kind == server::world::crafting::RecipeKind::Brewing) {
            continue;
        }
        if (listened && !known.contains(id) &&
            !events->allowed(&server::plugins::pluginEventTypes::playerRecipeDiscover,
                             [&](server::plugins::PluginEvent& event) {
                                 plugins::EventSink::player(event, session);
                                 event.set("recipe", id.toString());
                             })) {
            continue;
        }
        if (!progress->learn(uuid, id)) {
            continue;
        }
        learnt.push_back(id);
        if (triggers != nullptr) {
            triggers->onRecipeUnlocked(session.name(), uuid, id);
        }
    }
    if (!learnt.empty()) {
        world::recipeBook::add(session, *recipes, learnt);
    }
}

} // namespace basalt::server::runtime
