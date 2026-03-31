#include <basalt/server/world/crafting/Crafting.hpp>
#include <basalt/server/world/crafting/SpecialCrafting.hpp>

#include <algorithm>
#include <cstdint>
#include <utility>

namespace basalt::server::world::crafting {
namespace {

/// The four hooks, mirroring the mutable ones the frozen Kotlin keeps on its companion objects: what
/// they answer belongs to the item registry, which this module deliberately does not depend on.
hooks::RemainderOf& remainderHook() {
    static hooks::RemainderOf hook;
    return hook;
}

hooks::TrimMaterialOf& trimMaterialHook() {
    static hooks::TrimMaterialOf hook;
    return hook;
}

hooks::MaxDamageOf& maxDamageHook() {
    static hooks::MaxDamageOf hook;
    return hook;
}

hooks::DyeColourOf& dyeColourHook() {
    static hooks::DyeColourOf hook;
    return hook;
}

/// Whether every item can be assigned to a distinct ingredient it satisfies: a bipartite perfect
/// matching by augmenting paths. Vanilla answers the same question through `StackedContents.canCraft`;
/// craftability is a pure yes or no with no randomness in it, so any correct matching reaches the
/// same accept or reject.
bool tryAssign(std::size_t itemIndex, const std::vector<loot::ItemStack>& items,
               const std::vector<Ingredient>& ingredients, std::vector<std::int32_t>& assignedItem,
               std::vector<bool>& visited) {
    for (std::size_t index = 0; index < ingredients.size(); ++index) {
        if (visited[index] || !ingredients[index].test(&items[itemIndex])) {
            continue;
        }
        visited[index] = true;
        if (assignedItem[index] == -1 ||
            tryAssign(static_cast<std::size_t>(assignedItem[index]), items, ingredients, assignedItem, visited)) {
            assignedItem[index] = static_cast<std::int32_t>(itemIndex);
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool hasPerfectMatching(const std::vector<loot::ItemStack>& items,
                                      const std::vector<Ingredient>& ingredients) {
    std::vector<std::int32_t> assignedItem(ingredients.size(), -1);
    for (std::size_t itemIndex = 0; itemIndex < items.size(); ++itemIndex) {
        std::vector<bool> visited(ingredients.size(), false);
        if (!tryAssign(itemIndex, items, ingredients, assignedItem, visited)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::vector<loot::ItemStack> occupied(const CraftingInput& input) {
    std::vector<loot::ItemStack> stacks;
    for (std::int32_t y = 0; y < input.height(); ++y) {
        for (std::int32_t x = 0; x < input.width(); ++x) {
            if (const loot::ItemStack* stack = input.item(x, y); stack != nullptr) {
                stacks.push_back(*stack);
            }
        }
    }
    return stacks;
}

} // namespace

void hooks::setRemainderOf(RemainderOf hook) { remainderHook() = std::move(hook); }

void hooks::setTrimMaterialOf(TrimMaterialOf hook) { trimMaterialHook() = std::move(hook); }

void hooks::setMaxDamageOf(MaxDamageOf hook) { maxDamageHook() = std::move(hook); }

void hooks::setDyeColourOf(DyeColourOf hook) { dyeColourHook() = std::move(hook); }

std::optional<wire::common::Identifier> hooks::remainderOf(const wire::common::Identifier& item) {
    return remainderHook() ? remainderHook()(item) : std::nullopt;
}

std::optional<wire::common::Identifier> hooks::trimMaterialOf(const wire::common::Identifier& item) {
    return trimMaterialHook() ? trimMaterialHook()(item) : std::nullopt;
}

std::int32_t hooks::maxDamageOf(const wire::common::Identifier& item) {
    return maxDamageHook() ? maxDamageHook()(item) : 0;
}

std::optional<std::int32_t> hooks::dyeColourOf(const wire::common::Identifier& item) {
    return dyeColourHook() ? dyeColourHook()(item) : std::nullopt;
}

CraftingInput::CraftingInput(std::int32_t width, std::int32_t height, std::vector<std::optional<loot::ItemStack>> items)
    : width_(width), height_(height), items_(std::move(items)) {
    ingredientCount_ = static_cast<std::int32_t>(std::count_if(
        items_.begin(), items_.end(), [](const std::optional<loot::ItemStack>& slot) { return slot.has_value(); }));
}

const CraftingInput& CraftingInput::empty() {
    static const CraftingInput built{0, 0, {}};
    return built;
}

CraftingInput CraftingInput::of(std::int32_t width, std::int32_t height,
                                std::vector<std::optional<loot::ItemStack>> items) {
    if (width == 0 || height == 0) {
        return empty();
    }
    std::int32_t left = width - 1;
    std::int32_t right = 0;
    std::int32_t top = height - 1;
    std::int32_t bottom = 0;
    for (std::int32_t y = 0; y < height; ++y) {
        bool rowEmpty = true;
        for (std::int32_t x = 0; x < width; ++x) {
            if (items[static_cast<std::size_t>(x + y * width)].has_value()) {
                left = std::min(left, x);
                right = std::max(right, x);
                rowEmpty = false;
            }
        }
        if (!rowEmpty) {
            top = std::min(top, y);
            bottom = std::max(bottom, y);
        }
    }
    const std::int32_t newWidth = right - left + 1;
    const std::int32_t newHeight = bottom - top + 1;
    if (newWidth <= 0 || newHeight <= 0) {
        return empty();
    }
    if (newWidth == width && newHeight == height) {
        return CraftingInput{width, height, std::move(items)};
    }
    std::vector<std::optional<loot::ItemStack>> trimmed;
    trimmed.reserve(static_cast<std::size_t>(newWidth * newHeight));
    for (std::int32_t y = 0; y < newHeight; ++y) {
        for (std::int32_t x = 0; x < newWidth; ++x) {
            trimmed.push_back(items[static_cast<std::size_t>((x + left) + (y + top) * width)]);
        }
    }
    return CraftingInput{newWidth, newHeight, std::move(trimmed)};
}

const loot::ItemStack* CraftingInput::item(std::int32_t x, std::int32_t y) const {
    const std::size_t index = static_cast<std::size_t>(x + y * width_);
    if (index >= items_.size()) {
        return nullptr;
    }
    const std::optional<loot::ItemStack>& slot = items_[index];
    return slot.has_value() ? &*slot : nullptr;
}

bool Ingredient::test(const loot::ItemStack* stack) const {
    return stack != nullptr && items.contains(stack->item.toString());
}

bool Ingredient::testOptional(const std::optional<Ingredient>& ingredient, const loot::ItemStack* stack) {
    return ingredient.has_value() ? ingredient->test(stack) : stack == nullptr;
}

namespace {

/// parity: `Util.isSymmetrical`: a width-1 pattern is trivially symmetrical, otherwise every slot
/// must equal its horizontal mirror.
[[nodiscard]] bool isSymmetrical(const Recipe& recipe) {
    if (recipe.width == 1) {
        return true;
    }
    const std::int32_t centreX = recipe.width / 2;
    for (std::int32_t y = 0; y < recipe.height; ++y) {
        for (std::int32_t leftX = 0; leftX < centreX; ++leftX) {
            const std::int32_t rightX = recipe.width - 1 - leftX;
            if (recipe.shape[static_cast<std::size_t>(leftX + y * recipe.width)] !=
                recipe.shape[static_cast<std::size_t>(rightX + y * recipe.width)]) {
                return false;
            }
        }
    }
    return true;
}

[[nodiscard]] bool matchesFlip(const Recipe& recipe, const CraftingInput& input, bool xFlip) {
    for (std::int32_t y = 0; y < recipe.height; ++y) {
        for (std::int32_t x = 0; x < recipe.width; ++x) {
            const std::size_t index =
                static_cast<std::size_t>(xFlip ? recipe.width - x - 1 + y * recipe.width : x + y * recipe.width);
            if (!Ingredient::testOptional(recipe.shape[index], input.item(x, y))) {
                return false;
            }
        }
    }
    return true;
}

/// parity: `TransmuteRecipe.matches` and `assemble`: one input, one to eight materials, and the
/// input's components on whatever comes out.
[[nodiscard]] std::optional<loot::ItemStack> transmuted(const Recipe& recipe, const CraftingInput& input) {
    if (recipe.ingredients.size() != 2 || input.ingredientCount() < recipe.materialMin + 1 ||
        input.ingredientCount() > recipe.materialMax + 1) {
        return std::nullopt;
    }
    const loot::ItemStack* found = nullptr;
    std::int32_t materials = 0;
    for (std::int32_t y = 0; y < input.height(); ++y) {
        for (std::int32_t x = 0; x < input.width(); ++x) {
            const loot::ItemStack* stack = input.item(x, y);
            if (stack == nullptr) {
                continue;
            }
            if (recipe.ingredients[0].test(stack)) {
                if (found != nullptr) {
                    return std::nullopt;
                }
                found = stack;
            } else if (!recipe.ingredients[1].test(stack) || ++materials > recipe.materialMax) {
                return std::nullopt;
            }
        }
    }
    if (found == nullptr || materials < recipe.materialMin) {
        return std::nullopt;
    }
    const std::int32_t count = recipe.result.count + (recipe.addMaterialCount ? materials : 0);
    const wire::common::Identifier& item =
        recipe.result.item == wire::common::Identifier::vanilla("air") ? found->item : recipe.result.item;
    loot::ItemStack made = specialRecipes::withComponentsOf(item, count, *found);
    // parity: a one-for-one that changes nothing is no recipe at all.
    if (count == 1 && made.item == found->item && made.components == found->components) {
        return std::nullopt;
    }
    return made;
}

} // namespace

bool Recipe::matches(const CraftingInput& input) const {
    switch (kind) {
    case RecipeKind::Shaped: {
        const std::int32_t count = static_cast<std::int32_t>(std::count_if(
            shape.begin(), shape.end(), [](const std::optional<Ingredient>& slot) { return slot.has_value(); }));
        if (input.ingredientCount() != count) {
            return false;
        }
        if (input.width() != width || input.height() != height) {
            return false;
        }
        if (!isSymmetrical(*this) && matchesFlip(*this, input, true)) {
            return true;
        }
        return matchesFlip(*this, input, false);
    }
    case RecipeKind::Shapeless: {
        if (transmute) {
            return transmuted(*this, input).has_value();
        }
        if (input.ingredientCount() != static_cast<std::int32_t>(ingredients.size())) {
            return false;
        }
        return hasPerfectMatching(occupied(input), ingredients);
    }
    case RecipeKind::Brewing:
    case RecipeKind::Cooking:
    case RecipeKind::Stonecutting:
        // A cooking recipe or a stonecutter holds one item, so the grid form is one non-empty slot.
        return input.ingredientCount() == 1 && ingredient.test(input.item(0, 0));
    case RecipeKind::Smithing:
        // A smithing recipe is never a crafting-table recipe, whatever is in the grid.
        return false;
    case RecipeKind::Special:
    default:
        return specialCrafting::assemble(*this, input).has_value();
    }
}

loot::ItemStack Recipe::assemble(const CraftingInput& input) const {
    if (transmute) {
        return transmuted(*this, input).value_or(result);
    }
    if (kind != RecipeKind::Special) {
        return result;
    }
    return specialCrafting::assemble(*this, input).value_or(result);
}

std::vector<std::optional<loot::ItemStack>> Recipe::remainingItems(const CraftingInput& input) const {
    std::vector<std::optional<loot::ItemStack>> kept;
    for (std::int32_t y = 0; y < input.height(); ++y) {
        for (std::int32_t x = 0; x < input.width(); ++x) {
            const loot::ItemStack* stack = input.item(x, y);
            std::optional<loot::ItemStack> left;
            if (stack != nullptr) {
                if (const std::optional<wire::common::Identifier> remainder = hooks::remainderOf(stack->item);
                    remainder.has_value()) {
                    left = loot::ItemStack{.item = *remainder, .count = 1};
                }
            }
            kept.push_back(std::move(left));
        }
    }
    // parity: `BookCloningRecipe.getRemainingItems`, which puts the book being copied back: it is the
    // one recipe whose source survives being crafted with.
    if (kind != RecipeKind::Special || specialKind != SpecialKind::BookCloning) {
        return kept;
    }
    std::size_t slot = 0;
    for (std::int32_t y = 0; y < input.height(); ++y) {
        for (std::int32_t x = 0; x < input.width(); ++x) {
            const loot::ItemStack* stack = input.item(x, y);
            if (stack != nullptr && target.has_value() && target->test(stack)) {
                loot::ItemStack copy = *stack;
                copy.count = 1;
                kept[slot] = std::move(copy);
            }
            ++slot;
        }
    }
    return kept;
}

bool Recipe::accepts(const loot::ItemStack* stack) const { return ingredient.test(stack); }

bool Recipe::acceptsSmithing(const loot::ItemStack* templateStack, const loot::ItemStack* baseStack,
                             const loot::ItemStack* additionStack) const {
    return smithingTemplate.test(templateStack) && base.test(baseStack) && addition.test(additionStack);
}

std::optional<loot::ItemStack> Recipe::assembleSmithing(const loot::ItemStack& baseStack,
                                                        const loot::ItemStack& additionStack) const {
    if (!trimPattern.has_value()) {
        return specialRecipes::withComponentsOf(result.item, 1, baseStack);
    }
    const std::optional<wire::common::Identifier> trimMaterial = hooks::trimMaterialOf(additionStack.item);
    if (!trimMaterial.has_value()) {
        return std::nullopt;
    }
    wire::nbt::NbtCompound trim;
    trim.set("pattern", wire::nbt::NbtTag::ofString(trimPattern->toString()));
    trim.set("material", wire::nbt::NbtTag::ofString(trimMaterial->toString()));
    loot::ItemStack trimmed = specialRecipes::withComponentsOf(baseStack.item, 1, baseStack);
    // parity: applying the trim it already has is not a recipe at all.
    if (const wire::nbt::NbtTag* existing = trimmed.components.find(std::string{specialRecipes::kTrim});
        existing != nullptr && *existing == wire::nbt::NbtTag::ofCompound(trim)) {
        return std::nullopt;
    }
    trimmed.components.set(std::string{specialRecipes::kTrim}, wire::nbt::NbtTag::ofCompound(std::move(trim)));
    return trimmed;
}

} // namespace basalt::server::world::crafting
