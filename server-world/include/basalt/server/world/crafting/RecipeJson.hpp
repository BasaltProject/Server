#pragma once

#include <basalt/server/world/crafting/Crafting.hpp>

#include <expected>
#include <functional>
#include <set>
#include <string>
#include <string_view>

namespace basalt::server::world::crafting {

/// Why a recipe did not parse: a type this build does not understand, not malformed JSON.
struct RecipeParseError {
    std::string message;
};

/// Parses the real vanilla recipe JSON schema. Ingredients follow `HolderSetCodec`: a bare item
/// name, a `#tag` reference resolved through the caller, or a JSON array of either.
namespace recipeJson {

/// The item names a block or item tag stands for, resolved at decode time as vanilla does.
using TagResolver = std::function<std::set<std::string, std::less<>>(std::string_view)>;

[[nodiscard]] std::expected<Recipe, RecipeParseError> parse(std::string_view json, const TagResolver& resolveTag);

} // namespace recipeJson

} // namespace basalt::server::world::crafting
