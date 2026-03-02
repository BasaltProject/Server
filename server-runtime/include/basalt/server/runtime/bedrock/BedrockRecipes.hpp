#pragma once

#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/server/world/loot/Loot.hpp>
#include <basalt/wire/bedrock/BedrockPackets.hpp>

#include <cstddef>
#include <map>
#include <vector>

namespace basalt::server::runtime::world {
class RealRecipes;
}

namespace basalt::server::runtime::bedrock {

/// Every crafting recipe this server knows, in the shape that edition's clients read.
///
/// A Bedrock client crafts from the list the server sends it and from nothing else: with an empty
/// one the grid never shows a result, whatever is in it. Only the two kinds a table makes are sent;
/// cooking, stonecutting and smithing have screens of their own that this door does not draw yet.
struct BedrockRecipeList {
    std::vector<wire::bedrock::BedrockRecipe> recipes;
    /// What each of them makes, by the number it was sent under: a client asks to craft by that
    /// number and nothing else, so this is what turns the ask into a stack.
    std::map<std::int32_t, storage::StoredItemStack> results;
    /// How many a table could make but this door could not name: an item that edition has no name
    /// for makes the whole recipe unsendable.
    std::size_t unnamed = 0;
};

[[nodiscard]] BedrockRecipeList bedrockRecipes(const world::RealRecipes& recipes);

/// One stack as a trade carries it: the name that edition knows it by, how many, and which of them.
/// A stack this door has no name for comes back empty, which is how a client draws nothing.
[[nodiscard]] wire::bedrock::BedrockTradeItem tradeItemOf(const server::world::loot::ItemStack& stack);

} // namespace basalt::server::runtime::bedrock
