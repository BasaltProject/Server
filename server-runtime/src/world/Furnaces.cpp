#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/runtime/world/Furnaces.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <array>
#include <cmath>
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

/// parity: `AbstractFurnaceBlockEntity`'s own save keys.
constexpr std::string_view kLitRemainingKey = "lit_time_remaining";
constexpr std::string_view kLitTotalKey = "lit_total_time";
constexpr std::string_view kCookedKey = "cooking_time_spent";
constexpr std::string_view kCookTotalKey = "cooking_total_time";
constexpr std::string_view kRecipesUsedKey = "RecipesUsed";
constexpr std::string_view kSpeedKey = "speed_multiplier";

/// parity: `cooking/fast_burn_time_reduction_factor` and `cooking/fast_speed_multiplier`.
constexpr std::int32_t kFastBurnDivisor = 2;
constexpr float kFastSpeed = 2.0F;

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

/// parity: `canBurn`.
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
    // parity: the fuel slot is read as it stands now, so a lava bucket eaten this same tick leaves.
    if (ingredient.item == wetSponge() && state.items[kFuel].has_value() && state.items[kFuel]->item == bucket()) {
        state.items[kFuel] = ItemStack{.item = waterBucket(), .count = 1};
    }
    state.items[kInput] = shrink(ingredient);
}

/// parity: `consumeFuel`.
void consumeFuel(State& state, const ItemStack& fuel, const DropRemainder& drop) {
    std::optional<ItemStack> left = shrink(fuel);
    const wire::common::Identifier* remainder = registry::craftingRemainders::of(fuel.item);
    if (remainder != nullptr) {
        const ItemStack made{.item = *remainder, .count = 1};
        if (!left.has_value()) {
            left = made;
        } else if (drop) {
            drop(made);
        }
    }
    state.items[kFuel] = std::move(left);
}

/// parity: `getTotalCookTime(recipe, entity)`.
[[nodiscard]] std::int32_t atSpeed(std::int32_t cookingTime, float speed) {
    if (speed <= 0.0F) {
        return cookingTime;
    }
    return static_cast<std::int32_t>(std::ceil(static_cast<double>(static_cast<float>(cookingTime) / speed)));
}

/// parity: `getIntOr`.
[[nodiscard]] std::int32_t counterOf(const wire::nbt::NbtCompound& data, std::string_view key) {
    const wire::nbt::NbtTag* tag = data.find(key);
    if (tag == nullptr) {
        return 0;
    }
    if (const std::int32_t* whole = tag->get<std::int32_t>(); whole != nullptr) {
        return *whole;
    }
    const std::int16_t* narrow = tag->get<std::int16_t>();
    return narrow == nullptr ? 0 : *narrow;
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

FuelEffect fuelEffect(const wire::common::Identifier& fuel, Cooker cooker) {
    const std::int32_t plain = registry::fuels::burnTicks(fuel);
    // parity: no `cooking_fuel` at all reads both providers' defaults, 0 and 1.
    if (plain <= 0) {
        return FuelEffect{};
    }
    // parity: `cooking/time_*` is `div(ticks.
    const bool fast = cooker != Cooker::Smelting;
    return FuelEffect{.burnTicks = fast ? plain / kFastBurnDivisor : plain,
                      .speedMultiplier = fast ? kFastSpeed : 1.0F};
}

std::int32_t cookTimeFor(const State& state, const RecipeFor& recipeFor) {
    const std::optional<ItemStack>& ingredient = state.items[kInput];
    const Recipe* recipe = (ingredient.has_value() && recipeFor) ? recipeFor(*ingredient) : nullptr;
    return recipe == nullptr ? kDefaultCookTime : atSpeed(recipe->cookingTime, state.speedMultiplier);
}

Ticked tick(State& state, const RecipeFor& recipeFor, std::int32_t maxStackSize, Cooker cooker,
            const DropRemainder& drop) {
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
    // parity: `setItem` sets `cookingTotalTime` and zeroes the progress.
    if (ingredient.has_value()) {
        const std::int32_t wanted = cookTimeFor(state, recipeFor);
        if (state.cookingTotalTime != wanted) {
            state.cookingTotalTime = wanted;
            state.cookingTimer = 0;
            moved = true;
            changed = true;
        }
    }
    if (isLit || (fuel.has_value() && ingredient.has_value())) {
        if (ingredient.has_value()) {
            const Recipe* recipe = recipeFor ? recipeFor(*ingredient) : nullptr;
            // parity: an input nothing cooks leaves the progress where it stands. Only a recipe.
            if (recipe != nullptr) {
                if (recipe->result.count > 0 && canBurn(state, maxStackSize, recipe->result)) {
                    if (!isLit && fuel.has_value()) {
                        const FuelEffect effect = fuelEffect(fuel->item, cooker);
                        state.litTimeRemaining = effect.burnTicks;
                        state.litTotalTime = effect.burnTicks;
                        // parity: 26.3 takes the new fuel's speed even when it does not burn, and.
                        if (state.speedMultiplier != effect.speedMultiplier) {
                            moved = true;
                        }
                        state.speedMultiplier = effect.speedMultiplier;
                        if (state.cookingTotalTime > 0 && state.cookingTimer < state.cookingTotalTime) {
                            const float ratio =
                                static_cast<float>(state.cookingTimer) / static_cast<float>(state.cookingTotalTime);
                            const std::int32_t total = atSpeed(recipe->cookingTime, state.speedMultiplier);
                            const auto timer = static_cast<std::int32_t>(
                                std::ceil(static_cast<double>(ratio * static_cast<float>(total))));
                            moved = moved || total != state.cookingTotalTime || timer != state.cookingTimer;
                            state.cookingTotalTime = total;
                            state.cookingTimer = timer;
                        }
                        if (effect.burnTicks > 0) {
                            consumeFuel(state, *fuel, drop);
                            isLit = true;
                            moved = true;
                            changed = true;
                        }
                    }
                    if (isLit) {
                        ++state.cookingTimer;
                        moved = true;
                        if (state.cookingTimer >= state.cookingTotalTime) {
                            state.cookingTimer = 0;
                            state.cookingTotalTime = atSpeed(recipe->cookingTime, state.speedMultiplier);
                            burn(state, *ingredient, recipe->result);
                            // parity: `setRecipeUsed`, one more of this recipe to pay out later.
                            const auto used = std::ranges::find(state.recipesUsed, recipe->id,
                                                                &std::pair<std::string, std::int32_t>::first);
                            if (used == state.recipesUsed.end()) {
                                state.recipesUsed.emplace_back(recipe->id, 1);
                            } else {
                                ++used->second;
                            }
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
            state.cookingTimer = 0;
        }
    } else if (state.cookingTimer > 0) {
        // parity: `Mth.clamp`, whose upper bound wins when the counters disagree.
        const std::int32_t stepped = state.cookingTimer - kCoolingStep;
        state.cookingTimer = stepped < 0 ? 0 : std::min(stepped, state.cookingTotalTime);
        moved = true;
    }
    // parity: `wasLit != isLit` is the third and last thing that sets `changed`.
    return Ticked{.moved = moved, .changed = changed || wasLit != isLit};
}

void inputChanged(std::string_view block, wire::nbt::NbtCompound& data,
                  const std::function<const server::world::crafting::Recipe*(
                      server::world::crafting::Cooker, const server::world::loot::ItemStack&)>& recipeFor) {
    const std::optional<server::world::crafting::Cooker> cooker = cookerFor(block);
    if (!cooker.has_value() || !recipeFor) {
        return;
    }
    State state = read(data);
    state.cookingTimer = 0;
    state.cookingTotalTime = cookTimeFor(
        state, [&recipeFor, cooker](const server::world::loot::ItemStack& stack) { return recipeFor(*cooker, stack); });
    write(state, data);
}

std::int32_t experienceFor(std::int32_t count, float perItem, random::RandomSource& random) {
    const float total = static_cast<float>(count) * perItem;
    const std::int32_t whole = wire::common::mth::floor(static_cast<double>(total));
    std::int32_t reward = whole;
    const float fraction = total - static_cast<float>(whole);
    if (fraction != 0.0F && random.nextFloat() < fraction) {
        ++reward;
    }
    return reward;
}

RecipesUsed recipesUsedIn(const wire::nbt::NbtCompound& data) {
    RecipesUsed used;
    const wire::nbt::NbtCompound* saved = data.getCompound(kRecipesUsedKey);
    if (saved == nullptr) {
        return used;
    }
    for (const std::string& name : saved->names()) {
        used.emplace_back(name, saved->getInt(name));
    }
    return used;
}

State read(const wire::nbt::NbtCompound& data) {
    State state;
    state.litTimeRemaining = counterOf(data, kLitRemainingKey);
    state.litTotalTime = counterOf(data, kLitTotalKey);
    state.cookingTimer = counterOf(data, kCookedKey);
    state.cookingTotalTime = counterOf(data, kCookTotalKey);
    const wire::nbt::NbtTag* speed = data.find(kSpeedKey);
    const float* saved = speed == nullptr ? nullptr : speed->get<float>();
    state.speedMultiplier = saved == nullptr ? 1.0F : *saved;
    state.items = blockInventory::read(data, kSize);
    state.recipesUsed = recipesUsedIn(data);
    return state;
}

void write(const State& state, wire::nbt::NbtCompound& data) {
    data.putInt(kLitRemainingKey, state.litTimeRemaining);
    data.putInt(kLitTotalKey, state.litTotalTime);
    data.putInt(kCookedKey, state.cookingTimer);
    data.putInt(kCookTotalKey, state.cookingTotalTime);
    data.putFloat(kSpeedKey, state.speedMultiplier);
    blockInventory::write(data, state.items);
    wire::nbt::NbtCompound used;
    for (const auto& [name, count] : state.recipesUsed) {
        used.putInt(name, count);
    }
    data.set(kRecipesUsedKey, wire::nbt::NbtTag::ofCompound(std::move(used)));
}

} // namespace basalt::server::runtime::world::furnaces
