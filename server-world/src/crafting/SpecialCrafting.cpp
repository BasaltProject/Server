#include <basalt/server/world/crafting/SpecialCrafting.hpp>

#include <algorithm>
#include <cstdint>
#include <set>
#include <string>
#include <utility>

namespace basalt::server::world::crafting {
namespace {

using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;
using wire::nbt::NbtType;

/// parity: `RepairItemRecipe`, the extra five per cent of the bar a repair gives.
constexpr std::int32_t kRepairBonusPercent = 5;
/// parity: `Fireworks.flightDuration`, which is one to three and nothing else.
constexpr std::int32_t kMaxFlight = 3;

constexpr std::string_view kFireworkShape = "minecraft:firework_explosion_shape";
constexpr std::string_view kFireworkTrail = "minecraft:firework_explosion_trail";
constexpr std::string_view kFireworkTwinkle = "minecraft:firework_explosion_twinkle";
/// parity: `FireworkExplosion.colors`, the list of colours a star carries.
constexpr std::string_view kFireworkColours = "minecraft:firework_explosion_colors";
constexpr std::string_view kFlightDuration = "minecraft:fireworks_flight_duration";

/// parity: `FireworkExplosion.Shape`, the four a recipe can ask for on top of the small ball.
[[nodiscard]] const std::set<std::string, std::less<>>& shapes() {
    static const std::set<std::string, std::less<>> built{"burst", "creeper", "large_ball", "star"};
    return built;
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

[[nodiscard]] bool tests(const std::optional<Ingredient>& ingredient, const loot::ItemStack* stack) {
    return ingredient.has_value() && ingredient->test(stack);
}

[[nodiscard]] std::size_t patternCount(const loot::ItemStack& stack) {
    const NbtTag* patterns = stack.components.find(std::string{specialRecipes::kBannerPatterns});
    if (patterns == nullptr) {
        return 0;
    }
    const NbtList* list = patterns->get<NbtList>();
    return list == nullptr ? 0 : list->size();
}

/// parity: `RepairItemRecipe`, the only one whose two inputs are the same item.
[[nodiscard]] std::optional<loot::ItemStack> repair(const CraftingInput& input) {
    if (input.ingredientCount() != 2) {
        return std::nullopt;
    }
    const std::vector<loot::ItemStack> stacks = occupied(input);
    if (stacks.size() < 2 || stacks[0].count != 1 || stacks[1].count != 1) {
        return std::nullopt;
    }
    return specialRecipes::repair(stacks[0], stacks[1]);
}

/// parity: `ImbueRecipe` and `MapExtendingRecipe`: the thing in the middle, the eight around it the
/// same. They differ only in what comes out, which is why they share this.
[[nodiscard]] std::optional<loot::ItemStack> centredInNine(const Recipe& recipe, const CraftingInput& input,
                                                           bool keepResultCount) {
    if (input.width() != 3 || input.height() != 3 || input.ingredientCount() != 9) {
        return std::nullopt;
    }
    const loot::ItemStack* centre = input.item(1, 1);
    if (centre == nullptr || !tests(recipe.target, centre)) {
        return std::nullopt;
    }
    for (std::int32_t y = 0; y < 3; ++y) {
        for (std::int32_t x = 0; x < 3; ++x) {
            if (x == 1 && y == 1) {
                continue;
            }
            if (!tests(recipe.material, input.item(x, y))) {
                return std::nullopt;
            }
        }
    }
    return specialRecipes::withComponentsOf(recipe.result.item, keepResultCount ? recipe.result.count : 1, *centre);
}

/// parity: `DecoratedPotRecipe`, four sherds in a diamond and nothing in the corners.
[[nodiscard]] std::optional<loot::ItemStack> decoratedPot(const Recipe& recipe, const CraftingInput& input) {
    if (input.width() != 3 || input.height() != 3 || input.ingredientCount() != 4) {
        return std::nullopt;
    }
    for (const loot::ItemStack* side : {input.item(1, 0), input.item(0, 1), input.item(2, 1), input.item(1, 2)}) {
        if (!tests(recipe.material, side)) {
            return std::nullopt;
        }
    }
    return recipe.result;
}

/// parity: `DyeRecipe` and `FireworkStarFadeRecipe`: exactly one thing being coloured and at least
/// one dye, and the colours are averaged onto whatever it already had.
[[nodiscard]] std::optional<loot::ItemStack> dye(const Recipe& recipe, const CraftingInput& input) {
    if (input.ingredientCount() < 2) {
        return std::nullopt;
    }
    std::optional<loot::ItemStack> target;
    std::vector<std::int32_t> dyes;
    for (const loot::ItemStack& stack : occupied(input)) {
        if (tests(recipe.target, &stack) && !target.has_value()) {
            target = stack;
            continue;
        }
        if (!tests(recipe.material, &stack)) {
            return std::nullopt;
        }
        const std::optional<std::int32_t> colour = hooks::dyeColourOf(stack.item);
        if (!colour.has_value()) {
            return std::nullopt;
        }
        dyes.push_back(*colour);
    }
    if (!target.has_value() || dyes.empty()) {
        return std::nullopt;
    }
    std::optional<std::int32_t> current;
    if (const NbtTag* existing = target->components.find(std::string{specialRecipes::kDyedColor});
        existing != nullptr) {
        if (const std::int32_t* value = existing->get<std::int32_t>(); value != nullptr) {
            current = *value;
        }
    }
    loot::ItemStack coloured = specialRecipes::withComponentsOf(recipe.result.item, recipe.result.count, *target);
    coloured.components.set(std::string{specialRecipes::kDyedColor},
                            NbtTag::ofInt(specialRecipes::mixDyes(current, dyes)));
    return coloured;
}

/// parity: `BookCloningRecipe`: one written book and as many blank ones as you like.
[[nodiscard]] std::optional<loot::ItemStack> bookCloning(const Recipe& recipe, const CraftingInput& input) {
    if (input.ingredientCount() < 2) {
        return std::nullopt;
    }
    std::optional<loot::ItemStack> source;
    std::int32_t copies = 0;
    for (const loot::ItemStack& stack : occupied(input)) {
        if (tests(recipe.target, &stack) && !source.has_value()) {
            source = stack;
            continue;
        }
        if (!tests(recipe.material, &stack)) {
            return std::nullopt;
        }
        copies += stack.count;
    }
    if (!source.has_value() || copies < 1) {
        return std::nullopt;
    }
    // parity: the copies carry the book, which is the whole point of copying it.
    return specialRecipes::withComponentsOf(recipe.result.item, copies, *source);
}

/// parity: `BannerDuplicateRecipe`: one patterned banner and one blank of the same colour.
[[nodiscard]] std::optional<loot::ItemStack> bannerDuplicate(const Recipe& recipe, const CraftingInput& input) {
    if (input.ingredientCount() != 2) {
        return std::nullopt;
    }
    const std::vector<loot::ItemStack> stacks = occupied(input);
    for (const loot::ItemStack& stack : stacks) {
        if (!tests(recipe.target, &stack)) {
            return std::nullopt;
        }
    }
    const loot::ItemStack* patterned = nullptr;
    const loot::ItemStack* blank = nullptr;
    for (const loot::ItemStack& stack : stacks) {
        if (patterned == nullptr && patternCount(stack) != 0) {
            patterned = &stack;
        }
        if (blank == nullptr && patternCount(stack) == 0) {
            blank = &stack;
        }
    }
    if (patterned == nullptr || blank == nullptr || patterned == blank) {
        return std::nullopt;
    }
    return specialRecipes::withComponentsOf(recipe.result.item, 1, *patterned);
}

/// parity: `ShieldDecorationRecipe`: a plain shield and a banner, and the shield takes it on.
[[nodiscard]] std::optional<loot::ItemStack> shieldDecoration(const Recipe& recipe, const CraftingInput& input) {
    if (input.ingredientCount() != 2) {
        return std::nullopt;
    }
    const std::vector<loot::ItemStack> stacks = occupied(input);
    const loot::ItemStack* banner = nullptr;
    for (const loot::ItemStack& stack : stacks) {
        if (tests(recipe.material, &stack)) {
            banner = &stack;
            break;
        }
    }
    if (banner == nullptr) {
        return std::nullopt;
    }
    const loot::ItemStack* shield = nullptr;
    for (const loot::ItemStack& stack : stacks) {
        if (&stack != banner && tests(recipe.target, &stack)) {
            shield = &stack;
            break;
        }
    }
    if (shield == nullptr || patternCount(*shield) != 0) {
        return std::nullopt;
    }
    loot::ItemStack decorated = specialRecipes::withComponentsOf(recipe.result.item, 1, *shield);
    if (const NbtTag* patterns = banner->components.find(std::string{specialRecipes::kBannerPatterns});
        patterns != nullptr) {
        decorated.components.set(std::string{specialRecipes::kBannerPatterns}, *patterns);
    }
    return decorated;
}

/// parity: `FireworkStarRecipe`: one gunpowder, at least one dye, and any of the shape and effect
/// modifiers, each at most once.
[[nodiscard]] std::optional<loot::ItemStack> fireworkStar(const Recipe& recipe, const CraftingInput& input) {
    if (input.ingredientCount() < 2) {
        return std::nullopt;
    }
    bool gunpowder = false;
    std::vector<std::int32_t> dyes;
    std::set<std::string, std::less<>> extras;
    for (const loot::ItemStack& stack : occupied(input)) {
        const std::pair<std::string, Ingredient>* modifier = nullptr;
        for (const std::pair<std::string, Ingredient>& candidate : recipe.modifiers) {
            if (candidate.second.test(&stack)) {
                modifier = &candidate;
                break;
            }
        }
        if (tests(recipe.target, &stack) && !gunpowder) {
            gunpowder = true;
        } else if (tests(recipe.material, &stack)) {
            const std::optional<std::int32_t> colour = hooks::dyeColourOf(stack.item);
            if (!colour.has_value()) {
                return std::nullopt;
            }
            dyes.push_back(*colour);
        } else if (modifier != nullptr) {
            // parity: one shape, one trail and one twinkle at most, and a second of any of them
            // makes the whole thing not a recipe.
            if (!extras.insert(modifier->first).second) {
                return std::nullopt;
            }
        } else {
            return std::nullopt;
        }
    }
    if (!gunpowder || dyes.empty()) {
        return std::nullopt;
    }
    const std::size_t shapeCount = static_cast<std::size_t>(
        std::count_if(extras.begin(), extras.end(), [](const std::string& extra) { return shapes().contains(extra); }));
    if (shapeCount > 1) {
        return std::nullopt;
    }
    NbtCompound components;
    NbtList colours{NbtType::Int};
    for (const std::int32_t colour : dyes) {
        colours.push(NbtTag::ofInt(colour));
    }
    components.set(std::string{kFireworkColours}, NbtTag::ofList(std::move(colours)));
    for (const std::string& extra : extras) {
        if (shapes().contains(extra)) {
            components.set(std::string{kFireworkShape}, NbtTag::ofString(extra));
            break;
        }
    }
    if (extras.contains("trail")) {
        components.set(std::string{kFireworkTrail}, NbtTag::ofInt(1));
    }
    if (extras.contains("twinkle")) {
        components.set(std::string{kFireworkTwinkle}, NbtTag::ofInt(1));
    }
    return loot::ItemStack{
        .item = recipe.result.item, .count = recipe.result.count, .components = std::move(components)};
}

/// parity: `FireworkRocketRecipe`: one paper, one to three gunpowder, and any number of stars. The
/// flight time is the gunpowder count, and the stars ride along in the rocket.
[[nodiscard]] std::optional<loot::ItemStack> fireworkRocket(const Recipe& recipe, const CraftingInput& input) {
    if (input.ingredientCount() < 2) {
        return std::nullopt;
    }
    bool shell = false;
    std::int32_t fuel = 0;
    for (const loot::ItemStack& stack : occupied(input)) {
        if (tests(recipe.target, &stack) && !shell) {
            shell = true;
        } else if (tests(recipe.material, &stack)) {
            if (++fuel > kMaxFlight) {
                return std::nullopt;
            }
        } else if (tests(recipe.extra, &stack)) {
            continue;
        } else {
            return std::nullopt;
        }
    }
    if (!shell || fuel < 1) {
        return std::nullopt;
    }
    NbtCompound components;
    components.set(std::string{kFlightDuration}, NbtTag::ofInt(fuel));
    return loot::ItemStack{
        .item = recipe.result.item, .count = recipe.result.count, .components = std::move(components)};
}

} // namespace

std::optional<loot::ItemStack> specialCrafting::assemble(const Recipe& recipe, const CraftingInput& input) {
    switch (recipe.specialKind) {
    case SpecialKind::RepairItem:
        return repair(input);
    case SpecialKind::Imbue:
        // parity: the arrows come out carrying the potion that was in the middle.
        return centredInNine(recipe, input, true);
    case SpecialKind::DecoratedPot:
        return decoratedPot(recipe, input);
    case SpecialKind::MapExtending:
        return centredInNine(recipe, input, false);
    case SpecialKind::Dye:
    case SpecialKind::FireworkStarFade:
        return dye(recipe, input);
    case SpecialKind::BookCloning:
        return bookCloning(recipe, input);
    case SpecialKind::BannerDuplicate:
        return bannerDuplicate(recipe, input);
    case SpecialKind::ShieldDecoration:
        return shieldDecoration(recipe, input);
    case SpecialKind::FireworkStar:
        return fireworkStar(recipe, input);
    case SpecialKind::FireworkRocket:
    default:
        return fireworkRocket(recipe, input);
    }
}

std::int32_t specialRecipes::damageOf(const loot::ItemStack& stack) {
    const NbtTag* damage = stack.components.find(std::string{kDamage});
    if (damage == nullptr) {
        return 0;
    }
    const std::int32_t* value = damage->get<std::int32_t>();
    return value == nullptr ? 0 : *value;
}

std::optional<loot::ItemStack> specialRecipes::repair(const loot::ItemStack& first, const loot::ItemStack& second) {
    if (first.item != second.item) {
        return std::nullopt;
    }
    const std::int32_t durability = hooks::maxDamageOf(first.item);
    if (durability <= 0) {
        return std::nullopt;
    }
    const std::int32_t remaining =
        (durability - damageOf(first)) + (durability - damageOf(second)) + durability * kRepairBonusPercent / 100;
    NbtCompound components;
    components.set(std::string{kDamage}, NbtTag::ofInt(std::max(durability - remaining, 0)));
    return loot::ItemStack{.item = first.item, .count = 1, .components = std::move(components)};
}

std::int32_t specialRecipes::mixDyes(std::optional<std::int32_t> current, const std::vector<std::int32_t>& dyes) {
    if (dyes.empty()) {
        return current.value_or(0);
    }
    std::int32_t red = 0;
    std::int32_t green = 0;
    std::int32_t blue = 0;
    std::int32_t total = 0;
    std::int32_t count = 0;
    const auto fold = [&](std::int32_t colour) {
        const std::int32_t r = (colour >> 16) & 0xFF;
        const std::int32_t g = (colour >> 8) & 0xFF;
        const std::int32_t b = colour & 0xFF;
        total += std::max(r, std::max(g, b));
        red += r;
        green += g;
        blue += b;
        ++count;
    };
    if (current.has_value()) {
        fold(*current);
    }
    for (const std::int32_t dye : dyes) {
        fold(dye);
    }
    std::int32_t averageRed = red / count;
    std::int32_t averageGreen = green / count;
    std::int32_t averageBlue = blue / count;
    const float averageTotal = static_cast<float>(total) / static_cast<float>(count);
    const float brightest = static_cast<float>(std::max(averageRed, std::max(averageGreen, averageBlue)));
    if (brightest > 0.0F) {
        const float scale = averageTotal / brightest;
        averageRed = static_cast<std::int32_t>(static_cast<float>(averageRed) * scale);
        averageGreen = static_cast<std::int32_t>(static_cast<float>(averageGreen) * scale);
        averageBlue = static_cast<std::int32_t>(static_cast<float>(averageBlue) * scale);
    }
    return (averageRed << 16) | (averageGreen << 8) | averageBlue;
}

loot::ItemStack specialRecipes::withComponentsOf(const wire::common::Identifier& item, std::int32_t count,
                                                 const loot::ItemStack& from) {
    return loot::ItemStack{.item = item, .count = count, .components = from.components};
}

} // namespace basalt::server::world::crafting
