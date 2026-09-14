#pragma once

#include <basalt/wire/bedrock/BedrockPackets.hpp>

#include <cstddef>
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
    /// How many a table could make but this door could not name: an item that edition has no name
    /// for makes the whole recipe unsendable.
    std::size_t unnamed = 0;
};

[[nodiscard]] BedrockRecipeList bedrockRecipes(const world::RealRecipes& recipes);

} // namespace basalt::server::runtime::bedrock
