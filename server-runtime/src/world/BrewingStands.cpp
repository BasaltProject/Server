#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/world/BrewingStands.hpp>

#include <cstdint>

namespace basalt::server::runtime::world::brewingStands {
namespace {

constexpr std::string_view kFuelKey = "Fuel";
constexpr std::string_view kBrewTimeKey = "BrewTime";
constexpr std::string_view kPotionsKey = "BasaltPotions";

[[nodiscard]] std::string potionKey(std::int32_t slot) { return std::string{kPotionsKey} + std::to_string(slot); }

/// parity: `doBrew`: every bottle that can turn does, and the ingredient goes.
void brew(State& state) {
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
        state.items[index] = server::world::loot::ItemStack{.item = mixed->bottle, .count = state.items[index]->count};
        state.potions[index] = mixed->potion;
    }
    state.items[static_cast<std::size_t>(kIngredientSlot)] =
        ingredient->count <= 1
            ? std::nullopt
            : std::optional{server::world::loot::ItemStack{.item = ingredient->item, .count = ingredient->count - 1}};
}

} // namespace

const wire::common::Identifier& block() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("brewing_stand");
    return instance;
}

bool tick(State& state) {
    bool moved = false;
    const std::optional<server::world::loot::ItemStack>& fuelStack = state.items[static_cast<std::size_t>(kFuelSlot)];
    if (state.fuel <= 0 && fuelStack.has_value() && fuelStack->item == registry::brewing::fuel()) {
        state.fuel = registry::brewing::kFuelUses;
        state.items[static_cast<std::size_t>(kFuelSlot)] =
            fuelStack->count <= 1
                ? std::nullopt
                : std::optional{server::world::loot::ItemStack{.item = fuelStack->item, .count = fuelStack->count - 1}};
        moved = true;
    }
    const bool brewable = isBrewable(state);
    if (state.brewTime > 0) {
        --state.brewTime;
        if (state.brewTime == 0 && brewable) {
            brew(state);
        } else if (!brewable) {
            state.brewTime = 0;
        }
        return true;
    }
    if (brewable && state.fuel > 0) {
        --state.fuel;
        state.brewTime = registry::brewing::kBrewTime;
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
    // Read in vanilla's shapes, and in the int this build used to write, so a world of ours from
    // before does not lose its brew.
    state.fuel = data.getByte(kFuelKey, static_cast<std::int8_t>(data.getInt(kFuelKey, 0)));
    state.brewTime = data.getShort(kBrewTimeKey, static_cast<std::int16_t>(data.getInt(kBrewTimeKey, 0)));
    state.items = blockInventory::read(data, kSize);
    for (std::int32_t slot = 0; slot < kBottles; ++slot) {
        const std::string_view potion = data.getString(potionKey(slot));
        state.potions[static_cast<std::size_t>(slot)] =
            potion.empty() ? std::nullopt : std::optional{std::string{potion}};
    }
    return state;
}

void write(const State& state, wire::nbt::NbtCompound& data) {
    // parity: `saveAdditional` writes the brew time as a **short** and the fuel as a **byte**. A
    // vanilla reader asks for those types by name and a plain int comes back as the default, so the
    // two numbers have to go out in vanilla's own shapes or a world of ours loses them.
    data.putShort(kBrewTimeKey, static_cast<std::int16_t>(state.brewTime));
    data.putByte(kFuelKey, static_cast<std::int8_t>(state.fuel));
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
