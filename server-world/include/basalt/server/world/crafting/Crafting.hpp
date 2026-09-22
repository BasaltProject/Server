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

/// A crafting grid trimmed to the bounding box of its non-empty slots. parity:
/// `CraftingInput.ofPositioned`: recipes never see the raw grid position or its untrimmed size, only
/// the occupied shape, which is what makes a 2x2 recipe match anywhere in a 3x3 grid.
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

/// Accepts any item in `items`, a set already flattened from a tag reference if the recipe JSON used
/// one. parity: `Ingredient`, whose `HolderSet<Item>` is resolved the same way at decode time rather
/// than kept as a lazy tag lookup.
struct Ingredient {
    std::set<std::string, std::less<>> items;

    /// The tag this came from, when it came from one. Kept only so a client that has tags of its
    /// own can be told the tag rather than the eleven planks it stands for; nothing here matches
    /// on it, and an ingredient written as a list of items has none.
    std::string tag;

    [[nodiscard]] bool test(const loot::ItemStack* stack) const;

    /// parity: `Ingredient.testOptionalIngredient`: an empty slot matches only an absent ingredient.
    [[nodiscard]] static bool testOptional(const std::optional<Ingredient>& ingredient, const loot::ItemStack* stack);

    friend bool operator==(const Ingredient&, const Ingredient&) = default;
};

/// What each of the four blocks that cook accepts. parity: `AbstractCookingRecipe`'s subtypes.
enum class Cooker : std::uint8_t { Smelting, Blasting, Smoking, Campfire };

/// parity: the `crafting_special_*` family, `crafting_dye` and `crafting_imbue`: the ones whose
/// result depends on what went in rather than being a fixed stack.
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

/// A recipe: the two ingredient-based crafting-table types, the four cooking ones, the stonecutter's,
/// the two smithing ones and the eleven special ones, whose rule is code because their ingredients
/// are the file's. One tagged struct rather than a hierarchy: a recipe is data the loader reads.
struct Recipe {
    RecipeKind kind = RecipeKind::Shapeless;
    /// The name it was read under, which a furnace counts its cooking by.
    std::string id;
    std::string group;
    loot::ItemStack result;

    /// Shaped: the fixed grid, its cells absent where the pattern has a space.
    std::int32_t width = 0;
    std::int32_t height = 0;
    std::vector<std::optional<Ingredient>> shape;

    /// Shapeless: any arrangement, one distinct ingredient per item.
    std::vector<Ingredient> ingredients;

    /// Cooking and stonecutting: the one thing that goes in.
    Ingredient ingredient;
    Cooker cooker = Cooker::Smelting;
    float experience = 0.0F;
    std::int32_t cookingTime = 0;

    /// Smithing: the three slots, and the trim when the recipe applies one rather than transforming.
    Ingredient smithingTemplate;
    Ingredient base;
    Ingredient addition;
    std::optional<wire::common::Identifier> trimPattern;

    /// Brewing: the bottle in `base`, the potions it may hold (ids or one `#tag`, empty for any), and
    /// the reagent in `ingredient`. parity: 26.3 `BrewingRecipe`, whose output is `result`.
    std::vector<std::string> potions;

    /// Special: the rule, plus whichever of its ingredient fields the file carries.
    SpecialKind specialKind = SpecialKind::RepairItem;
    std::optional<Ingredient> target;
    std::optional<Ingredient> material;
    std::optional<Ingredient> extra;
    /// The named extras a firework star takes, each at most once: the four shapes, trail, twinkle.
    std::vector<std::pair<std::string, Ingredient>> modifiers;

    [[nodiscard]] bool matches(const CraftingInput& input) const;

    /// What this recipe makes of `input`. parity: `Recipe.assemble`. Nearly every recipe answers with
    /// its own fixed result; the ones that do not are exactly the specials.
    [[nodiscard]] loot::ItemStack assemble(const CraftingInput& input) const;

    /// What each grid cell is left holding once this is crafted. parity: `Recipe.getRemainingItems`,
    /// which is what puts the empty bucket back where the milk was.
    [[nodiscard]] std::vector<std::optional<loot::ItemStack>> remainingItems(const CraftingInput& input) const;

    /// Cooking and stonecutting hold one item, so this is the direct form of the question.
    [[nodiscard]] bool accepts(const loot::ItemStack* stack) const;

    /// parity: `SmithingTransformRecipe.assemble` and `SmithingTrimRecipe.applyTrim`.
    [[nodiscard]] std::optional<loot::ItemStack> assembleSmithing(const loot::ItemStack& base,
                                                                  const loot::ItemStack& addition) const;

    [[nodiscard]] bool acceptsSmithing(const loot::ItemStack* templateStack, const loot::ItemStack* baseStack,
                                       const loot::ItemStack* additionStack) const;
};

/// The four questions a recipe asks the rest of the server, filled in by the caller. parity: vanilla
/// answers them from the item registry, which this module deliberately does not depend on.
namespace hooks {

/// What an item leaves behind when crafted with, such as a bucket.
using RemainderOf = std::function<std::optional<wire::common::Identifier>(const wire::common::Identifier&)>;
/// parity: `DataComponents.PROVIDES_TRIM_MATERIAL`, a component on the ingot rather than a list.
using TrimMaterialOf = std::function<std::optional<wire::common::Identifier>(const wire::common::Identifier&)>;
/// How much damage an item of this kind takes before it breaks.
using MaxDamageOf = std::function<std::int32_t(const wire::common::Identifier&)>;
/// The colour each dye item stands for.
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
