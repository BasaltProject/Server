#pragma once

#include <basalt/server/world/crafting/Crafting.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::runtime::world {

/// Every crafting recipe the operator's own `server.jar` carries, read once at startup, like
/// `RealLootTables` and `RealNoiseParameters`: datapack content is never committed here.
///
/// Item tags load first, because an ingredient can name one and the parser flattens it as it reads.
/// A recipe type this build has no model for is skipped rather than failing the load: refusing to
/// start over an uncraftable firework would be the worse answer.
class RealRecipes {
public:
    /// One jar entry as text, or nothing when the jar has no such entry.
    using ReadText = std::function<std::optional<std::string>(std::string_view name)>;

    /// Every entry name under a prefix, which is how a whole data directory is listed.
    using EntriesUnder = std::function<std::vector<std::string>(std::string_view prefix)>;

    /// The jar arrives as callbacks rather than as `ServerJarAssets`: this needs the operator's own
    /// copy of the game, and a test has no jar to hand it.
    struct Jar {
        ReadText readText;
        EntriesUnder entriesUnder;
    };

    /// A recipe that matched, with the name it was read under.
    ///
    /// The name is what `limited_crafting` checks against the player's recipe book, which is the
    /// whole reason a match needs one rather than just a result.
    struct Match {
        wire::common::Identifier id;
        const server::world::crafting::Recipe* recipe = nullptr;
    };

    explicit RealRecipes(const Jar& jar, wire::common::Logger logger = wire::common::Logger{"Recipes"});

    /// How many recipes were understood, for the startup line that reports it.
    [[nodiscard]] std::size_t size() const noexcept { return recipes_.size(); }

    /// Every recipe file's name, whether or not this build could parse it: what `/recipe *` names.
    [[nodiscard]] const std::set<wire::common::Identifier>& ids() const noexcept { return ids_; }

    /// The recipe behind a name, for the book entries `/recipe give` sends.
    ///
    /// Only a recipe this build parsed has one: a type it skipped can be *named* by `/recipe` (its
    /// file is there) but cannot be drawn, and is left out of the book rather than drawn wrong.
    [[nodiscard]] const server::world::crafting::Recipe* recipeFor(const wire::common::Identifier& id) const;

    /// What `cooker` makes of a stack, or nothing when it makes nothing.
    ///
    /// parity: `RecipeManager.getRecipeFor`, which walks the recipes of that type and takes the
    /// first that accepts what is in the slot.
    [[nodiscard]] const server::world::crafting::Recipe* cookingFor(server::world::crafting::Cooker cooker,
                                                                    const server::world::loot::ItemStack& stack) const;

    /// Every stonecutting recipe, in the order they were read.
    ///
    /// The order is the contract with the client: it filters this same list by what is in the input
    /// slot and sends back an index into what is left, so both sides have to agree on it.
    [[nodiscard]] const std::vector<const server::world::crafting::Recipe*>& stonecutting() const noexcept {
        return stonecutting_;
    }

    /// What a stonecutter can make of a stack, in the order the client will draw the buttons.
    [[nodiscard]] std::vector<const server::world::crafting::Recipe*>
    stonecuttingFor(const server::world::loot::ItemStack& stack) const;

    /// Every smithing recipe, which the smithing table walks in order like the stonecutter does.
    [[nodiscard]] const std::vector<const server::world::crafting::Recipe*>& smithing() const noexcept {
        return smithing_;
    }

    /// Every `minecraft:brewing` recipe, in the order they were read.
    [[nodiscard]] const std::vector<const server::world::crafting::Recipe*>& brewing() const noexcept {
        return brewing_;
    }

    /// What a smithing table makes of these three slots, or nothing when they are not a recipe.
    [[nodiscard]] const server::world::crafting::Recipe*
    smithingFor(const server::world::loot::ItemStack* templateStack, const server::world::loot::ItemStack* baseStack,
                const server::world::loot::ItemStack* additionStack) const;

    /// The name a recipe was read under, for the criteria that watch what was crafted. Only a
    /// pointer this object owns has one.
    [[nodiscard]] const wire::common::Identifier* nameOf(const server::world::crafting::Recipe* recipe) const;

    /// The recipe this grid matches, with its name.
    [[nodiscard]] std::optional<Match> recipeMatching(const server::world::crafting::CraftingInput& input) const;

    /// What this grid layout crafts right now, or nothing if it is not a recipe.
    ///
    /// parity: `Recipe.assemble`, not `getResultItem`: a dye, a copy or a repair is defined by what
    /// it carries over from the grid, so its result cannot be a fixed stack.
    [[nodiscard]] std::optional<server::world::loot::ItemStack>
    resultFor(const server::world::crafting::CraftingInput& input) const;

private:
    void load(const Jar& jar);

    wire::common::Logger logger_;
    std::vector<server::world::crafting::Recipe> recipes_;
    /// The name each parsed recipe came from, in the same order, so a match can be named.
    std::vector<wire::common::Identifier> parsedIds_;
    std::set<wire::common::Identifier> ids_;
    std::map<server::world::crafting::Cooker, std::vector<const server::world::crafting::Recipe*>> cooking_;
    std::vector<const server::world::crafting::Recipe*> stonecutting_;
    std::vector<const server::world::crafting::Recipe*> smithing_;
    std::vector<const server::world::crafting::Recipe*> brewing_;
};

} // namespace basalt::server::runtime::world
