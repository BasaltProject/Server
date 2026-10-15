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

struct BedrockRecipeList {
    std::vector<wire::bedrock::BedrockRecipe> recipes;
    std::map<std::int32_t, storage::StoredItemStack> results;
    std::size_t unnamed = 0;
};

[[nodiscard]] BedrockRecipeList bedrockRecipes(const world::RealRecipes& recipes);

[[nodiscard]] wire::bedrock::BedrockTradeItem tradeItemOf(const server::world::loot::ItemStack& stack);

} // namespace basalt::server::runtime::bedrock
