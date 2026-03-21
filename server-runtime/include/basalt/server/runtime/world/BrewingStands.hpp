#pragma once

#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace basalt::server::runtime::world {

/// The brewing stand, one tick at a time.
///
/// parity: `BrewingStandBlockEntity.serverTick`, in its order: the powder is eaten first and only
/// when the fuel has run out, a brew that stops matching its ingredient is abandoned rather than
/// finished, and the three bottle bits on the block follow whatever is standing in it.
///
/// The potion a bottle holds is a component in vanilla; here it is kept beside the item in the block
/// entity's own NBT, which is enough to brew with and is marked as the shortcut it is.
namespace brewingStands {

/// parity: `BrewingStandBlockEntity`'s five slots: three bottles, the ingredient and the fuel.
inline constexpr std::int32_t kSize = 5;
inline constexpr std::int32_t kBottles = 3;
inline constexpr std::int32_t kIngredientSlot = 3;
inline constexpr std::int32_t kFuelSlot = 4;

[[nodiscard]] const wire::common::Identifier& block();

/// A stand as it stands: five slots, the fuel left and how far along the brew is.
struct State {
    std::int32_t fuel = 0;
    std::int32_t brewTime = 0;
    /// parity: 26.3's `totalBrewTime`, `totalFuel` and `speedMultiplier`, with its load defaults.
    std::int32_t totalBrewTime = 400;
    std::int32_t totalFuel = 20;
    float speedMultiplier = 1.0F;
    /// parity: `ingredient`, what the brew in hand started with; a different one stops it.
    std::optional<wire::common::Identifier> ingredient;
    blockInventory::Slots items = blockInventory::Slots(kSize);
    std::array<std::optional<std::string>, kBottles> potions;
};

/// Where a remainder goes when its slot still holds more of what made it. parity:
/// `Containers.dropItemStack` at the stand, which only the caller can do.
using DropRemainder = std::function<void(const server::world::loot::ItemStack&)>;

/// One turn. Answers whether anything moved, so the caller writes it back only when it did.
bool tick(State& state, const DropRemainder& drop = {});

/// parity: `isBrewable`: an ingredient that brews, and at least one bottle it can turn.
[[nodiscard]] bool isBrewable(const State& state);

/// Which of the three bottle slots are taken, which is what the block itself shows.
[[nodiscard]] std::array<bool, kBottles> bottleBits(const State& state);

[[nodiscard]] State read(const wire::nbt::NbtCompound& data);

void write(const State& state, wire::nbt::NbtCompound& data);

} // namespace brewingStands

} // namespace basalt::server::runtime::world
