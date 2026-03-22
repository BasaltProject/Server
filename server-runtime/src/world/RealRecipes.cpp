#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/registry/Tags.hpp>
#include <basalt/server/runtime/world/RealRecipes.hpp>
#include <basalt/server/world/crafting/RecipeJson.hpp>

#include <algorithm>
#include <cstdint>

namespace basalt::server::runtime::world {
namespace {

using wire::common::Identifier;
namespace crafting = server::world::crafting;

constexpr std::string_view kRecipePrefix = "data/minecraft/recipe/";
constexpr std::string_view kItemTagPrefix = "data/minecraft/tags/item/";
constexpr std::string_view kJsonSuffix = ".json";

[[nodiscard]] std::string stripped(const std::string& entry, std::string_view prefix) {
    std::string path = entry;
    if (path.starts_with(prefix)) {
        path.erase(0, prefix.size());
    }
    if (path.ends_with(kJsonSuffix)) {
        path.resize(path.size() - kJsonSuffix.size());
    }
    return path;
}

/// The item tag graph, flattened.
///
/// A tag whose members this build's registry does not know is dropped rather than fatal: the
/// registry is generated from the same jar, so a mismatch there means something else is already
/// wrong, and refusing to craft anything is not the way to say so.
[[nodiscard]] std::map<Identifier, std::set<Identifier>> loadItemTags(const RealRecipes::Jar& jar,
                                                                      const wire::common::Logger& logger) {
    std::map<Identifier, registry::tags::TagFile> files;
    for (const std::string& entry : jar.entriesUnder(kItemTagPrefix)) {
        if (!entry.ends_with(kJsonSuffix)) {
            continue;
        }
        const std::optional<std::string> text = jar.readText(entry);
        if (!text.has_value()) {
            logger.debug([&] { return "Skipped item tag " + entry + ": no such entry"; });
            continue;
        }
        const std::expected<registry::tags::TagFile, registry::tags::tagFileJson::ParseError> file =
            registry::tags::tagFileJson::parse(*text);
        if (!file.has_value()) {
            logger.debug([&] { return "Skipped item tag " + entry + ": malformed"; });
            continue;
        }
        files.emplace(Identifier::vanilla(stripped(entry, kItemTagPrefix)), *file);
    }
    const std::expected<std::map<Identifier, std::set<Identifier>>, registry::tags::TagError> resolved =
        registry::tags::resolve(files,
                                [](const Identifier& id) { return registry::items::registry().byName(id) != nullptr; });
    if (!resolved.has_value()) {
        logger.warn([&] {
            return "Item tags could not be resolved, so tag-based recipes will not match: " +
                   resolved.error().message();
        });
        return {};
    }
    return *resolved;
}

} // namespace

RealRecipes::RealRecipes(const Jar& jar, wire::common::Logger logger) : logger_(std::move(logger)) {
    if (jar.readText && jar.entriesUnder) {
        load(jar);
    }
    for (const crafting::Recipe& recipe : recipes_) {
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

    std::vector<std::string> entries = jar.entriesUnder(kRecipePrefix);
    std::sort(entries.begin(), entries.end());
    std::int32_t skipped = 0;
    // Reserved up front: a parsed recipe is handed out by pointer, so the vector must not grow later.
    recipes_.reserve(entries.size());
    for (const std::string& entry : entries) {
        if (!entry.ends_with(kJsonSuffix)) {
            continue;
        }
        const Identifier id = Identifier::vanilla(stripped(entry, kRecipePrefix));
        ids_.insert(id);
        const std::optional<std::string> text = jar.readText(entry);
        // A type with no model here, or a file this build could not read: both are named, because a
        // silent skip is how a whole family of recipes went missing without anyone noticing.
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
    }
    logger_.info([&] {
        return "Loaded " + std::to_string(recipes_.size()) + " crafting recipe(s) (" + std::to_string(skipped) +
               " other recipe type(s) skipped).";
    });
}

const crafting::Recipe* RealRecipes::recipeFor(const Identifier& id) const {
    const auto found = std::ranges::find(parsedIds_, id);
    return found == parsedIds_.end() ? nullptr : &recipes_[static_cast<std::size_t>(found - parsedIds_.begin())];
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
    if (recipe == nullptr || recipes_.empty() || recipe < recipes_.data() ||
        recipe >= recipes_.data() + recipes_.size()) {
        return nullptr;
    }
    const auto index = static_cast<std::size_t>(recipe - recipes_.data());
    return index < parsedIds_.size() ? &parsedIds_[index] : nullptr;
}

std::optional<RealRecipes::Match> RealRecipes::recipeMatching(const crafting::CraftingInput& input) const {
    for (std::size_t index = 0; index < recipes_.size(); ++index) {
        // parity: `RecipeManager.getRecipeFor(RecipeType.CRAFTING, ...)`. Everything the jar ships
        // lives in one folder, cooking and stonecutting included, and only the grid's own types may
        // answer here. Without the filter a log in a crafting grid came out as **charcoal**, because
        // `charcoal.json` sorts before `oak_planks.json` and a smelting recipe takes one ingredient
        // just as happily. Found by the bench circuit `crafter`.
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
