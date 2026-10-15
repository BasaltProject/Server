#pragma once

#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/world/crafting/Crafting.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::runtime::world {

namespace brewingStands {

inline constexpr std::int32_t kSize = 5;
inline constexpr std::int32_t kBottles = 3;
inline constexpr std::int32_t kIngredientSlot = 3;
inline constexpr std::int32_t kFuelSlot = 4;

[[nodiscard]] const wire::common::Identifier& block();

struct State {
    std::int32_t fuel = 0;
    std::int32_t brewTime = 0;
    std::int32_t totalBrewTime = 400;
    std::int32_t totalFuel = 20;
    float speedMultiplier = 1.0F;
    std::optional<wire::common::Identifier> ingredient;
    blockInventory::Slots items = blockInventory::Slots(kSize);
    std::array<std::optional<std::string>, kBottles> potions;
};

using DropRemainder = std::function<void(const server::world::loot::ItemStack&)>;

using PotionTagMembers = std::function<std::vector<wire::common::Identifier>(const wire::common::Identifier& tag)>;

void useRecipes(std::vector<server::world::crafting::Recipe> recipes, PotionTagMembers potionTags = {});

[[nodiscard]] std::optional<server::world::loot::ItemStack> brewed(const server::world::loot::ItemStack& bottle,
                                                                   const std::optional<std::string>& potion,
                                                                   const wire::common::Identifier& reagent);

[[nodiscard]] bool isReagent(const wire::common::Identifier& item);

[[nodiscard]] bool isPotionInput(const wire::common::Identifier& item);

/// One turn. Answers whether anything moved, so the caller writes it back only when it did.
bool tick(State& state, const DropRemainder& drop = {});

[[nodiscard]] bool isBrewable(const State& state);

[[nodiscard]] std::array<bool, kBottles> bottleBits(const State& state);

[[nodiscard]] State read(const wire::nbt::NbtCompound& data);

void write(const State& state, wire::nbt::NbtCompound& data);

} // namespace brewingStands

} // namespace basalt::server::runtime::world
