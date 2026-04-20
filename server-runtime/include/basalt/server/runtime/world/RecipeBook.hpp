#pragma once

#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/RealRecipes.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/protocol/play/RecipePackets.hpp>

#include <optional>
#include <vector>

namespace basalt::server::runtime::world::recipeBook {

/// Shared because `/recipe give` and an advancement reward both unlock recipes and must put the same entry in the book.

void add(const player::PlayerSession& target, const RealRecipes& recipes,
         const std::vector<wire::common::Identifier>& ids);

void remove(const player::PlayerSession& target, const RealRecipes& recipes,
            const std::vector<wire::common::Identifier>& ids);

/// parity: `ServerRecipeBook.sendInitialRecipeBook`.
void resend(const player::PlayerSession& target, const RealRecipes& recipes,
            const std::vector<wire::common::Identifier>& ids);

[[nodiscard]] std::optional<wire::protocol::play::RecipeBookEntry> entryFor(const RealRecipes& recipes,
                                                                            const wire::common::Identifier& id);

} // namespace basalt::server::runtime::world::recipeBook
