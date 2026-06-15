#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/registry/Tags.hpp>
#include <basalt/server/runtime/datapack/DataFiles.hpp>
#include <basalt/server/runtime/world/RealRecipes.hpp>
#include <basalt/server/world/crafting/RecipeJson.hpp>

#include <algorithm>
#include <cstdint>

namespace basalt::server::runtime::world {
namespace {

using wire::common::Identifier;
namespace crafting = server::world::crafting;

[[nodiscard]] std::map<Identifier, std::set<Identifier>> loadItemTags(const RealRecipes::Jar& jar,
                                                                      const wire::common::Logger& logger) {
    return datapack::resolveTags(
        jar.entriesUnder, jar.readText, "item",
        [](const Identifier& id) { return registry::items::registry().byName(id) != nullptr; }, logger);
}

} // namespace

RealRecipes::RealRecipes(const Jar& jar, wire::common::Logger logger) : logger_(std::move(logger)), jar_(jar) {
    if (jar.readText && jar.entriesUnder) {
        load(jar);
    }
    index();
}

void RealRecipes::index() {
    cooking_.clear();
    stonecutting_.clear();
    smithing_.clear();
    brewing_.clear();
    for (std::size_t at = 0; at < recipes_.size(); ++at) {
        if (!live_[at]) {
            continue;
        }
        const crafting::Recipe& recipe = recipes_[at];
        switch (recipe.kind) {
        case crafting::RecipeKind::Cooking:
            cooking_[recipe.cooker].push_back(&recipe);
            break;
        case crafting::RecipeKind::Stonecutting:
            stonecutting_.push_back(&recipe);
            break;
        case crafting::RecipeKind::Smithing:
            smithing_.push_back(&recipe);
            break;
        case crafting::RecipeKind::Brewing:
            brewing_.push_back(&recipe);
            break;
        default:
            break;
        }
    }
}

void RealRecipes::load(const Jar& jar) {
    const std::map<Identifier, std::set<Identifier>> tags = loadItemTags(jar, logger_);
    const crafting::recipeJson::TagResolver resolveTag = [&tags](std::string_view name) {
        std::set<std::string, std::less<>> members;
        const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(name);
        if (!id.has_value()) {
            return members;
        }
        const auto found = tags.find(*id);
        if (found == tags.end()) {
            return members;
        }
        for (const Identifier& member : found->second) {
            members.insert(member.toString());
        }
        return members;
    };

    const std::vector<std::pair<Identifier, std::string>> entries = datapack::dataFiles(jar.entriesUnder, "recipe");
    std::int32_t skipped = 0;
    for (const auto& [id, entry] : entries) {
        ids_.insert(id);
        const std::optional<std::string> text = jar.readText(entry);
        if (!text.has_value()) {
            logger_.debug([&] { return "Skipped " + entry + ": no such entry"; });
            ++skipped;
            continue;
        }
        const std::expected<crafting::Recipe, crafting::RecipeParseError> recipe =
            crafting::recipeJson::parse(*text, resolveTag);
        if (!recipe.has_value()) {
            logger_.debug([&] { return "Skipped " + entry + ": " + recipe.error().message; });
            ++skipped;
            continue;
        }
        recipes_.push_back(*recipe);
        recipes_.back().id = id.toString();
        parsedIds_.push_back(id);
        live_.push_back(true);
    }
    logger_.info([&] {
        return "Loaded " + std::to_string(recipes_.size()) + " crafting recipe(s) (" + std::to_string(skipped) +
               " other recipe type(s) skipped).";
    });
}

const crafting::Recipe* RealRecipes::recipeFor(const Identifier& id) const {
    for (std::size_t at = 0; at < parsedIds_.size(); ++at) {
        if (live_[at] && parsedIds_[at] == id) {
            return &recipes_[at];
        }
    }
    return nullptr;
}

bool RealRecipes::add(const Identifier& id, crafting::Recipe recipe) {
    if (ids_.contains(id)) {
        return false;
    }
    recipe.id = id.toString();
    recipes_.push_back(std::move(recipe));
    parsedIds_.push_back(id);
    live_.push_back(true);
    ids_.insert(id);
    index();
    return true;
}

bool RealRecipes::remove(const Identifier& id) {
    bool removed = false;
    for (std::size_t at = 0; at < parsedIds_.size(); ++at) {
        if (live_[at] && parsedIds_[at] == id) {
            live_[at] = false;
            removed = true;
        }
    }
    if (ids_.erase(id) > 0 || removed) {
        index();
        return true;
    }
    return false;
}

std::vector<Identifier> RealRecipes::listed() const {
    std::vector<Identifier> out;
    for (std::size_t at = 0; at < parsedIds_.size(); ++at) {
        if (live_[at]) {
            out.push_back(parsedIds_[at]);
        }
    }
    return out;
}

void RealRecipes::clear() {
    std::ranges::fill(live_, false);
    ids_.clear();
    index();
}

void RealRecipes::restore() {
    recipes_.clear();
    parsedIds_.clear();
    live_.clear();
    ids_.clear();
    if (jar_.readText && jar_.entriesUnder) {
        load(jar_);
    }
    index();
}

const std::vector<const crafting::Recipe*>& RealRecipes::cooking(crafting::Cooker cooker) const {
    static const std::vector<const crafting::Recipe*> none;
    const auto found = cooking_.find(cooker);
    return found == cooking_.end() ? none : found->second;
}

const crafting::Recipe* RealRecipes::cookingFor(crafting::Cooker cooker,
                                                const server::world::loot::ItemStack& stack) const {
    const auto found = cooking_.find(cooker);
    if (found == cooking_.end()) {
        return nullptr;
    }
    for (const crafting::Recipe* recipe : found->second) {
        if (recipe->accepts(&stack)) {
            return recipe;
        }
    }
    return nullptr;
}

std::vector<const crafting::Recipe*> RealRecipes::stonecuttingFor(const server::world::loot::ItemStack& stack) const {
    std::vector<const crafting::Recipe*> matching;
    for (const crafting::Recipe* recipe : stonecutting_) {
        if (recipe->accepts(&stack)) {
            matching.push_back(recipe);
        }
    }
    return matching;
}

const crafting::Recipe* RealRecipes::smithingFor(const server::world::loot::ItemStack* templateStack,
                                                 const server::world::loot::ItemStack* baseStack,
                                                 const server::world::loot::ItemStack* additionStack) const {
    for (const crafting::Recipe* recipe : smithing_) {
        if (recipe->acceptsSmithing(templateStack, baseStack, additionStack)) {
            return recipe;
        }
    }
    return nullptr;
}

const Identifier* RealRecipes::nameOf(const crafting::Recipe* recipe) const {
    for (std::size_t at = 0; at < recipes_.size(); ++at) {
        if (&recipes_[at] == recipe) {
            return &parsedIds_[at];
        }
    }
    return nullptr;
}

std::optional<RealRecipes::Match> RealRecipes::recipeMatching(const crafting::CraftingInput& input) const {
    for (std::size_t index = 0; index < recipes_.size(); ++index) {
        if (!live_[index]) {
            continue;
        }
        // parity: `RecipeManager.getRecipeFor(RecipeType.CRAFTING, ...)`.
        const crafting::RecipeKind kind = recipes_[index].kind;
        if (kind != crafting::RecipeKind::Shaped && kind != crafting::RecipeKind::Shapeless &&
            kind != crafting::RecipeKind::Special) {
            continue;
        }
        if (recipes_[index].matches(input)) {
            return Match{.id = parsedIds_[index], .recipe = &recipes_[index]};
        }
    }
    return std::nullopt;
}

std::optional<server::world::loot::ItemStack> RealRecipes::resultFor(const crafting::CraftingInput& input) const {
    const std::optional<Match> match = recipeMatching(input);
    return match.has_value() ? std::optional{match->recipe->assemble(input)} : std::nullopt;
}

} // namespace basalt::server::runtime::world
