#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/runtime/world/BrewingStands.hpp>

#include <cmath>
#include <cstdint>
#include <expected>

namespace basalt::server::runtime::world::brewingStands {
namespace {

constexpr std::string_view kFuelKey = "Fuel";
constexpr std::string_view kBrewTimeKey = "BrewTime";
constexpr std::string_view kPotionsKey = "BasaltPotions";
constexpr std::string_view kTotalBrewTimeKey = "total_brew_time";
constexpr std::string_view kTotalFuelKey = "total_fuel";
constexpr std::string_view kSpeedKey = "speed_multiplier";
/// Vanilla keeps the brew's ingredient in memory only; a stand here is read back every turn.
constexpr std::string_view kIngredientKey = "BasaltIngredient";
constexpr std::string_view kPotionContents = "minecraft:potion_contents";

using ItemStack = server::world::loot::ItemStack;

/// One fewer, and the remainder in its place or dropped. parity: 26.3 reads the remainder before
/// the shrink, so the last dragon's breath leaves its bottle, where 26.2 read it off the empty stack.
[[nodiscard]] std::optional<ItemStack> spend(const ItemStack& stack, const DropRemainder& drop) {
    const wire::common::Identifier* remainder = registry::craftingRemainders::of(stack.item);
    std::optional<ItemStack> left =
        stack.count <= 1 ? std::nullopt : std::optional{ItemStack{.item = stack.item, .count = stack.count - 1}};
    if (remainder != nullptr) {
        const ItemStack made{.item = *remainder, .count = 1};
        if (!left.has_value()) {
            left = made;
        } else if (drop) {
            drop(made);
        }
    }
    return left;
}

/// Reads an int that older saves of this build or of 26.2 wrote narrower.
[[nodiscard]] std::int32_t wideOf(const wire::nbt::NbtCompound& data, std::string_view key, std::int32_t fallback) {
    const wire::nbt::NbtTag* tag = data.find(key);
    if (tag == nullptr) {
        return fallback;
    }
    if (const std::int32_t* whole = tag->get<std::int32_t>(); whole != nullptr) {
        return *whole;
    }
    if (const std::int16_t* narrow = tag->get<std::int16_t>(); narrow != nullptr) {
        return *narrow;
    }
    const std::int8_t* tiny = tag->get<std::int8_t>();
    return tiny == nullptr ? fallback : *tiny;
}

[[nodiscard]] std::string potionKey(std::int32_t slot) { return std::string{kPotionsKey} + std::to_string(slot); }

/// parity: `doBrew`: every bottle that can turn does, and the ingredient goes.
void brew(State& state, const DropRemainder& drop) {
    const std::optional<server::world::loot::ItemStack>& ingredient =
        state.items[static_cast<std::size_t>(kIngredientSlot)];
    if (!ingredient.has_value()) {
        return;
    }
    for (std::int32_t slot = 0; slot < kBottles; ++slot) {
        const auto index = static_cast<std::size_t>(slot);
        if (!state.items[index].has_value()) {
            continue;
        }
        const std::optional<registry::brewing::Brew> mixed =
            registry::brewing::mix(state.items[index]->item, state.potions[index], ingredient->item);
        if (!mixed.has_value()) {
            continue;
        }
        server::world::loot::ItemStack turned{.item = mixed->bottle, .count = state.items[index]->count};
        if (mixed->potion.has_value()) {
            wire::nbt::NbtCompound contents;
            contents.putString("potion", "minecraft:" + *mixed->potion);
            turned.components.set(kPotionContents, wire::nbt::NbtTag::ofCompound(std::move(contents)));
        }
        state.items[index] = std::move(turned);
        state.potions[index] = mixed->potion;
    }
    state.items[static_cast<std::size_t>(kIngredientSlot)] = spend(*ingredient, drop);
}

} // namespace

const wire::common::Identifier& block() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("brewing_stand");
    return instance;
}

bool tick(State& state, const DropRemainder& drop) {
    bool moved = false;
    const std::optional<ItemStack>& fuelStack = state.items[static_cast<std::size_t>(kFuelSlot)];
    // parity: the `minecraft:brewing_fuel` component, whose `brewing/uses_default` is twenty and
    // `brewing/speed_default` is one.
    if (state.fuel <= 0 && fuelStack.has_value() && fuelStack->item == registry::brewing::fuel()) {
        state.fuel = registry::brewing::kFuelUses;
        state.totalFuel = state.fuel;
        state.speedMultiplier = 1.0F;
        state.items[static_cast<std::size_t>(kFuelSlot)] = spend(*fuelStack, drop);
        moved = true;
    }
    const bool brewable = isBrewable(state);
    const std::optional<ItemStack>& ingredient = state.items[static_cast<std::size_t>(kIngredientSlot)];
    if (state.brewTime > 0) {
        --state.brewTime;
        if (state.brewTime == 0 && brewable) {
            brew(state, drop);
        } else if (!brewable || !ingredient.has_value() || ingredient->item != state.ingredient) {
            state.brewTime = 0;
        }
        return true;
    }
    if (brewable && state.fuel > 0) {
        const float speed = state.speedMultiplier > 0.0F ? state.speedMultiplier : 1.0F;
        --state.fuel;
        state.brewTime = static_cast<std::int32_t>(
            std::ceil(static_cast<double>(static_cast<float>(registry::brewing::kBrewTime) / speed)));
        state.totalBrewTime = state.brewTime;
        state.ingredient = ingredient->item;
        return true;
    }
    return moved;
}

bool isBrewable(const State& state) {
    const std::optional<server::world::loot::ItemStack>& ingredient =
        state.items[static_cast<std::size_t>(kIngredientSlot)];
    if (!ingredient.has_value() || !registry::brewing::isIngredient(ingredient->item)) {
        return false;
    }
    for (std::int32_t slot = 0; slot < kBottles; ++slot) {
        const auto index = static_cast<std::size_t>(slot);
        if (!state.items[index].has_value()) {
            continue;
        }
        if (registry::brewing::mix(state.items[index]->item, state.potions[index], ingredient->item).has_value()) {
            return true;
        }
    }
    return false;
}

std::array<bool, kBottles> bottleBits(const State& state) {
    std::array<bool, kBottles> bits{};
    for (std::int32_t slot = 0; slot < kBottles; ++slot) {
        bits[static_cast<std::size_t>(slot)] = state.items[static_cast<std::size_t>(slot)].has_value();
    }
    return bits;
}

State read(const wire::nbt::NbtCompound& data) {
    State state;
    // parity: 26.3 `loadAdditional`, ints now; 26.2 wrote a byte and a short, taken as well.
    state.fuel = wideOf(data, kFuelKey, 0);
    state.brewTime = wideOf(data, kBrewTimeKey, 0);
    state.totalBrewTime = wideOf(data, kTotalBrewTimeKey, registry::brewing::kBrewTime);
    state.totalFuel = wideOf(data, kTotalFuelKey, registry::brewing::kFuelUses);
    state.speedMultiplier = data.getFloat(kSpeedKey, 1.0F);
    state.items = blockInventory::read(data, kSize);
    // parity: a loaded stand mid-brew takes the ingredient in its slot as the one it started with.
    const std::expected<wire::common::Identifier, wire::common::IdentifierError> started =
        wire::common::Identifier::parse(data.getString(kIngredientKey));
    if (started.has_value()) {
        state.ingredient = *started;
    } else if (state.brewTime > 0 && state.items[static_cast<std::size_t>(kIngredientSlot)].has_value()) {
        state.ingredient = state.items[static_cast<std::size_t>(kIngredientSlot)]->item;
    }
    for (std::int32_t slot = 0; slot < kBottles; ++slot) {
        // The component is the potion; the stand's own key only speaks for bottles saved before it.
        const std::optional<ItemStack>& bottle = state.items[static_cast<std::size_t>(slot)];
        const wire::nbt::NbtCompound* contents =
            bottle.has_value() ? bottle->components.getCompound(kPotionContents) : nullptr;
        std::string_view potion = !bottle.has_value() || bottle->item.path() != "glass_bottle"
                                      ? data.getString(potionKey(slot))
                                      : std::string_view{};
        if (contents != nullptr) {
            potion = contents->getString("potion");
            if (potion.starts_with("minecraft:")) {
                potion.remove_prefix(std::string_view{"minecraft:"}.size());
            }
        }
        state.potions[static_cast<std::size_t>(slot)] =
            potion.empty() ? std::nullopt : std::optional{std::string{potion}};
    }
    return state;
}

void write(const State& state, wire::nbt::NbtCompound& data) {
    // parity: 26.3 `saveAdditional`, every counter an int.
    data.putInt(kBrewTimeKey, state.brewTime);
    data.putInt(kTotalBrewTimeKey, state.totalBrewTime);
    data.putInt(kFuelKey, state.fuel);
    data.putInt(kTotalFuelKey, state.totalFuel);
    data.putFloat(kSpeedKey, state.speedMultiplier);
    if (state.ingredient.has_value() && state.brewTime > 0) {
        data.putString(kIngredientKey, state.ingredient->toString());
    } else {
        data.remove(kIngredientKey);
    }
    blockInventory::write(data, state.items);
    for (std::int32_t slot = 0; slot < kBottles; ++slot) {
        const std::optional<std::string>& potion = state.potions[static_cast<std::size_t>(slot)];
        if (potion.has_value()) {
            data.putString(potionKey(slot), *potion);
        } else {
            data.remove(potionKey(slot));
        }
    }
}

} // namespace basalt::server::runtime::world::brewingStands
