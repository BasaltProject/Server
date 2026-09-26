#pragma once

#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/RealRecipes.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/protocol/play/RecipePackets.hpp>

#include <optional>
#include <vector>

namespace basalt::server::runtime::world::recipeBook {

/// One player's recipe book, as the client draws it.
///
/// Shared because two things unlock a recipe: `/recipe give`, and an advancement whose reward is a
/// recipe, which is how vanilla unlocks most of the book. Both have to put the same entry in it.

/// Puts recipes in a player's book, skipping any this build did not parse and so cannot draw.
void add(const player::PlayerSession& target, const RealRecipes& recipes,
         const std::vector<wire::common::Identifier>& ids);

void remove(const player::PlayerSession& target, const RealRecipes& recipes,
            const std::vector<wire::common::Identifier>& ids);

/// The whole book again, replacing what the client had: its numbers change when recipes reload.
/// parity: `ServerRecipeBook.sendInitialRecipeBook`.
void resend(const player::PlayerSession& target, const RealRecipes& recipes,
            const std::vector<wire::common::Identifier>& ids);

/// One book entry, or nothing for a recipe this build did not parse and so cannot draw.
[[nodiscard]] std::optional<wire::protocol::play::RecipeBookEntry> entryFor(const RealRecipes& recipes,
                                                                            const wire::common::Identifier& id);

} // namespace basalt::server::runtime::world::recipeBook
