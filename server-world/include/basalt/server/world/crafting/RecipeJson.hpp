#pragma once

#include <basalt/server/world/crafting/Crafting.hpp>

#include <expected>
#include <functional>
#include <set>
#include <string>
#include <string_view>

namespace basalt::server::world::crafting {

struct RecipeParseError {
    std::string message;
};

namespace recipeJson {

using TagResolver = std::function<std::set<std::string, std::less<>>(std::string_view)>;

[[nodiscard]] std::expected<Recipe, RecipeParseError> parse(std::string_view json, const TagResolver& resolveTag);

} // namespace recipeJson

} // namespace basalt::server::world::crafting
