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

class RealRecipes {
public:
    using ReadText = std::function<std::optional<std::string>(std::string_view name)>;

    using EntriesUnder = std::function<std::vector<std::string>(std::string_view prefix)>;

    struct Jar {
        ReadText readText;
        EntriesUnder entriesUnder;
    };

    /// The name is what `limited_crafting` checks against the player's recipe book.
    struct Match {
        wire::common::Identifier id;
        const server::world::crafting::Recipe* recipe = nullptr;
    };

    explicit RealRecipes(const Jar& jar, wire::common::Logger logger = wire::common::Logger{"Recipes"});

    [[nodiscard]] std::size_t size() const noexcept { return recipes_.size(); }

    [[nodiscard]] const std::set<wire::common::Identifier>& ids() const noexcept { return ids_; }

    /// A type this build skipped can be named by `/recipe` but not drawn, so it is left out of the book.
    [[nodiscard]] const server::world::crafting::Recipe* recipeFor(const wire::common::Identifier& id) const;

    /// parity: `RecipeManager.getRecipeFor` takes the first recipe of that type that accepts the slot.
    [[nodiscard]] const server::world::crafting::Recipe* cookingFor(server::world::crafting::Cooker cooker,
                                                                    const server::world::loot::ItemStack& stack) const;

    /// The order is the contract with the client, which sends back an index into its filtered copy of this list.
    [[nodiscard]] const std::vector<const server::world::crafting::Recipe*>& stonecutting() const noexcept {
        return stonecutting_;
    }

    [[nodiscard]] std::vector<const server::world::crafting::Recipe*>
    stonecuttingFor(const server::world::loot::ItemStack& stack) const;

    [[nodiscard]] const std::vector<const server::world::crafting::Recipe*>& smithing() const noexcept {
        return smithing_;
    }

    [[nodiscard]] const std::vector<const server::world::crafting::Recipe*>&
    cooking(server::world::crafting::Cooker cooker) const;

    [[nodiscard]] const std::vector<const server::world::crafting::Recipe*>& brewing() const noexcept {
        return brewing_;
    }

    [[nodiscard]] const server::world::crafting::Recipe*
    smithingFor(const server::world::loot::ItemStack* templateStack, const server::world::loot::ItemStack* baseStack,
                const server::world::loot::ItemStack* additionStack) const;

    [[nodiscard]] const wire::common::Identifier* nameOf(const server::world::crafting::Recipe* recipe) const;

    [[nodiscard]] std::optional<Match> recipeMatching(const server::world::crafting::CraftingInput& input) const;

    /// parity: `Recipe.assemble`.
    [[nodiscard]] std::optional<server::world::loot::ItemStack>
    resultFor(const server::world::crafting::CraftingInput& input) const;

private:
    void load(const Jar& jar);

    wire::common::Logger logger_;
    std::vector<server::world::crafting::Recipe> recipes_;
    std::vector<wire::common::Identifier> parsedIds_;
    std::set<wire::common::Identifier> ids_;
    std::map<server::world::crafting::Cooker, std::vector<const server::world::crafting::Recipe*>> cooking_;
    std::vector<const server::world::crafting::Recipe*> stonecutting_;
    std::vector<const server::world::crafting::Recipe*> smithing_;
    std::vector<const server::world::crafting::Recipe*> brewing_;
};

} // namespace basalt::server::runtime::world
