#pragma once

#include <basalt/server/world/loot/Loot.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace basalt::server::world::crafting {

class CraftingInput {
public:
    [[nodiscard]] static const CraftingInput& empty();

    [[nodiscard]] static CraftingInput of(std::int32_t width, std::int32_t height,
                                          std::vector<std::optional<loot::ItemStack>> items);

    [[nodiscard]] std::int32_t width() const noexcept { return width_; }

    [[nodiscard]] std::int32_t height() const noexcept { return height_; }

    [[nodiscard]] std::int32_t ingredientCount() const noexcept { return ingredientCount_; }

    [[nodiscard]] const loot::ItemStack* item(std::int32_t x, std::int32_t y) const;

private:
    CraftingInput(std::int32_t width, std::int32_t height, std::vector<std::optional<loot::ItemStack>> items);

    std::int32_t width_ = 0;
    std::int32_t height_ = 0;
    std::int32_t ingredientCount_ = 0;
    std::vector<std::optional<loot::ItemStack>> items_;
};

/// one. parity: `Ingredient`, whose `HolderSet<Item>` is resolved the same way at decode time rather
struct Ingredient {
    std::set<std::string, std::less<>> items;

    std::string tag;

    [[nodiscard]] bool test(const loot::ItemStack* stack) const;

    /// parity: `Ingredient.testOptionalIngredient`.
    [[nodiscard]] static bool testOptional(const std::optional<Ingredient>& ingredient, const loot::ItemStack* stack);

    friend bool operator==(const Ingredient&, const Ingredient&) = default;
};

/// What each of the four blocks that cook accepts. parity: `AbstractCookingRecipe`'s subtypes.
enum class Cooker : std::uint8_t { Smelting, Blasting, Smoking, Campfire };

/// parity: the `crafting_special_*` family.
enum class SpecialKind : std::uint8_t {
    Dye,
    Imbue,
    BookCloning,
    BannerDuplicate,
    FireworkRocket,
    FireworkStar,
    FireworkStarFade,
    MapExtending,
    ShieldDecoration,
    RepairItem,
    DecoratedPot,
};

enum class RecipeKind : std::uint8_t { Shaped, Shapeless, Cooking, Stonecutting, Smithing, Special, Brewing };

struct Recipe {
    RecipeKind kind = RecipeKind::Shapeless;
    std::string id;
    std::string group;
    loot::ItemStack result;

    std::int32_t width = 0;
    std::int32_t height = 0;
    std::vector<std::optional<Ingredient>> shape;

    std::vector<Ingredient> ingredients;

    Ingredient ingredient;
    Cooker cooker = Cooker::Smelting;
    float experience = 0.0F;
    std::int32_t cookingTime = 0;

    Ingredient smithingTemplate;
    Ingredient base;
    Ingredient addition;
    std::optional<wire::common::Identifier> trimPattern;

    /// the reagent in `ingredient`. parity: 26.3 `BrewingRecipe`, whose output is `result`.
    std::vector<std::string> potions;

    /// Transmute: a shapeless pair whose result keeps the input's components. parity: `TransmuteRecipe`.
    bool transmute = false;
    std::int32_t materialMin = 1;
    std::int32_t materialMax = 1;
    bool addMaterialCount = false;
    SpecialKind specialKind = SpecialKind::RepairItem;
    std::optional<Ingredient> target;
    std::optional<Ingredient> material;
    std::optional<Ingredient> extra;
    std::vector<std::pair<std::string, Ingredient>> modifiers;

    [[nodiscard]] bool matches(const CraftingInput& input) const;

    /// What this recipe makes of `input`. parity: `Recipe.assemble`. Nearly every recipe answers with
    [[nodiscard]] loot::ItemStack assemble(const CraftingInput& input) const;

    /// What each grid cell is left holding once this is crafted. parity: `Recipe.getRemainingItems`.
    [[nodiscard]] std::vector<std::optional<loot::ItemStack>> remainingItems(const CraftingInput& input) const;

    [[nodiscard]] bool accepts(const loot::ItemStack* stack) const;

    /// parity: `SmithingTransformRecipe.assemble` and `SmithingTrimRecipe.applyTrim`.
    [[nodiscard]] std::optional<loot::ItemStack> assembleSmithing(const loot::ItemStack& base,
                                                                  const loot::ItemStack& addition) const;

    [[nodiscard]] bool acceptsSmithing(const loot::ItemStack* templateStack, const loot::ItemStack* baseStack,
                                       const loot::ItemStack* additionStack) const;
};

/// The four questions a recipe asks the rest of the server, filled in by the caller. parity: vanilla
namespace hooks {

using RemainderOf = std::function<std::optional<wire::common::Identifier>(const wire::common::Identifier&)>;
/// parity: `DataComponents.PROVIDES_TRIM_MATERIAL`.
using TrimMaterialOf = std::function<std::optional<wire::common::Identifier>(const wire::common::Identifier&)>;
using MaxDamageOf = std::function<std::int32_t(const wire::common::Identifier&)>;
using DyeColourOf = std::function<std::optional<std::int32_t>(const wire::common::Identifier&)>;

void setRemainderOf(RemainderOf hook);
void setTrimMaterialOf(TrimMaterialOf hook);
void setMaxDamageOf(MaxDamageOf hook);
void setDyeColourOf(DyeColourOf hook);

[[nodiscard]] std::optional<wire::common::Identifier> remainderOf(const wire::common::Identifier& item);
[[nodiscard]] std::optional<wire::common::Identifier> trimMaterialOf(const wire::common::Identifier& item);
[[nodiscard]] std::int32_t maxDamageOf(const wire::common::Identifier& item);
[[nodiscard]] std::optional<std::int32_t> dyeColourOf(const wire::common::Identifier& item);

} // namespace hooks

} // namespace basalt::server::world::crafting
