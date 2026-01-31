#include <basalt/server/registry/Items.hpp>
#include <basalt/server/runtime/world/Furnaces.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <utility>

namespace basalt::server::runtime::world::furnaces {
namespace {

using ItemStack = server::world::loot::ItemStack;
using server::world::crafting::Cooker;
using server::world::crafting::Recipe;

constexpr std::size_t kInput = static_cast<std::size_t>(kInputSlot);
constexpr std::size_t kFuel = static_cast<std::size_t>(kFuelSlot);
constexpr std::size_t kResult = static_cast<std::size_t>(kResultSlot);

/// parity: `AbstractFurnaceBlockEntity`'s own save keys, which it writes as shorts.
constexpr std::string_view kLitRemainingKey = "lit_time_remaining";
constexpr std::string_view kLitTotalKey = "lit_total_time";
constexpr std::string_view kCookedKey = "cooking_time_spent";
constexpr std::string_view kCookTotalKey = "cooking_total_time";

/// parity: the two ticks a cooling furnace walks its progress back by.
constexpr std::int32_t kCoolingStep = 2;

constexpr std::array<std::pair<std::string_view, Cooker>, 3> kCookers = {{
    {"furnace", Cooker::Smelting},
    {"blast_furnace", Cooker::Blasting},
    {"smoker", Cooker::Smoking},
}};

[[nodiscard]] const wire::common::Identifier& wetSponge() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("wet_sponge");
    return instance;
}

[[nodiscard]] const wire::common::Identifier& bucket() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("bucket");
    return instance;
}

[[nodiscard]] const wire::common::Identifier& waterBucket() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("water_bucket");
    return instance;
}

[[nodiscard]] std::optional<ItemStack> shrink(const ItemStack& stack) {
    if (stack.count <= 1) {
        return std::nullopt;
    }
    ItemStack left = stack;
    left.count = stack.count - 1;
    return left;
}

/// parity: `canBurn`: an empty result slot always takes it, a matching one takes it while it fits
/// under both the furnace's ceiling and the result item's own.
[[nodiscard]] bool canBurn(const State& state, std::int32_t maxStackSize, const ItemStack& result) {
    const std::optional<ItemStack>& inResult = state.items[kResult];
    if (!inResult.has_value()) {
        return true;
    }
    if (inResult->item != result.item || inResult->components != result.components) {
        return false;
    }
    return inResult->count + result.count <= std::min(maxStackSize, registry::itemStacking::maxStackSize(result.item));
}

/// parity: `burn`, including the bucket a wet sponge fills as it dries.
void burn(State& state, const ItemStack& ingredient, const ItemStack& result) {
    if (state.items[kResult].has_value()) {
        state.items[kResult]->count += result.count;
    } else {
        state.items[kResult] = result;
    }
    // parity: the fuel slot is read as it stands now, so a lava bucket eaten this same tick leaves
    // the bucket the sponge then fills.
    if (ingredient.item == wetSponge() && state.items[kFuel].has_value() && state.items[kFuel]->item == bucket()) {
        state.items[kFuel] = ItemStack{.item = waterBucket(), .count = 1};
    }
    state.items[kInput] = shrink(ingredient);
}

/// parity: `consumeFuel`: a bucket of lava leaves the bucket behind.
void consumeFuel(State& state, const ItemStack& fuel) {
    std::optional<ItemStack> left = shrink(fuel);
    if (!left.has_value()) {
        const wire::common::Identifier* remainder = registry::craftingRemainders::of(fuel.item);
        if (remainder != nullptr) {
            left = ItemStack{.item = *remainder, .count = 1};
        }
    }
    state.items[kFuel] = std::move(left);
}

} // namespace

std::optional<Cooker> cookerFor(std::string_view block) {
    for (const auto& [name, cooker] : kCookers) {
        if (name == block) {
            return cooker;
        }
    }
    return std::nullopt;
}

Ticked tick(State& state, const RecipeFor& recipeFor, std::int32_t maxStackSize) {
    bool moved = false;
    bool changed = false;
    const bool wasLit = state.litTimeRemaining > 0;
    bool isLit = wasLit;
    if (wasLit) {
        --state.litTimeRemaining;
        isLit = state.litTimeRemaining > 0;
        moved = true;
    }
    const std::optional<ItemStack>& fuel = state.items[kFuel];
    const std::optional<ItemStack>& ingredient = state.items[kInput];
    if (isLit || (fuel.has_value() && ingredient.has_value())) {
        if (ingredient.has_value()) {
            const Recipe* recipe = recipeFor ? recipeFor(*ingredient) : nullptr;
            // parity: an input nothing cooks leaves the progress where it stands. Only a recipe
            // whose result does not fit walks it back to zero.
            if (recipe != nullptr) {
                if (recipe->result.count > 0 && canBurn(state, maxStackSize, recipe->result)) {
                    if (!isLit && fuel.has_value()) {
                        const std::int32_t burnTicks = registry::fuels::burnTicks(fuel->item);
                        state.litTimeRemaining = burnTicks;
                        state.litTotalTime = burnTicks;
                        if (burnTicks > 0) {
                            consumeFuel(state, *fuel);
                            isLit = true;
                            moved = true;
                            changed = true;
                        }
                    }
                    if (isLit) {
                        ++state.cookingTimer;
                        moved = true;
                        if (state.cookingTimer == state.cookingTotalTime) {
                            state.cookingTimer = 0;
                            state.cookingTotalTime = recipe->cookingTime;
                            burn(state, *ingredient, recipe->result);
                            changed = true;
                        }
                    } else {
                        moved = moved || state.cookingTimer != 0;
                        state.cookingTimer = 0;
                    }
                } else {
                    moved = moved || state.cookingTimer != 0;
                    state.cookingTimer = 0;
                }
            }
        } else {
            // Nothing in the input and the fire still on, which is the only way here, so whatever
            // the progress was the tick above already counts as movement.
            state.cookingTimer = 0;
        }
    } else if (state.cookingTimer > 0) {
        // parity: `Mth.clamp`, whose upper bound wins when the counters disagree.
        const std::int32_t stepped = state.cookingTimer - kCoolingStep;
        state.cookingTimer = stepped < 0 ? 0 : std::min(stepped, state.cookingTotalTime);
        moved = true;
    }
    // parity: `wasLit != isLit` is the third and last thing that sets `changed`, and it is also what
    // writes the block's own `lit`.
    return Ticked{.moved = moved, .changed = changed || wasLit != isLit};
}

std::int32_t cookTimeFor(const State& state, const RecipeFor& recipeFor) {
    const std::optional<ItemStack>& ingredient = state.items[kInput];
    const Recipe* recipe = (ingredient.has_value() && recipeFor) ? recipeFor(*ingredient) : nullptr;
    return recipe == nullptr ? kDefaultCookTime : recipe->cookingTime;
}

State read(const wire::nbt::NbtCompound& data) {
    State state;
    state.litTimeRemaining = data.getShort(kLitRemainingKey);
    state.litTotalTime = data.getShort(kLitTotalKey);
    state.cookingTimer = data.getShort(kCookedKey);
    state.cookingTotalTime = data.getShort(kCookTotalKey);
    state.items = blockInventory::read(data, kSize);
    return state;
}

void write(const State& state, wire::nbt::NbtCompound& data) {
    data.putShort(kLitRemainingKey, static_cast<std::int16_t>(state.litTimeRemaining));
    data.putShort(kLitTotalKey, static_cast<std::int16_t>(state.litTotalTime));
    data.putShort(kCookedKey, static_cast<std::int16_t>(state.cookingTimer));
    data.putShort(kCookTotalKey, static_cast<std::int16_t>(state.cookingTotalTime));
    blockInventory::write(data, state.items);
}

} // namespace basalt::server::runtime::world::furnaces
