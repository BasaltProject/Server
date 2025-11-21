#pragma once

#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/world/crafting/Crafting.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// The campfire, which cooks four things at once and hands each one back on the ground.
///
/// parity: `CampfireBlockEntity.cookTick` while it is lit and `cooldownTick` while it is not: what
/// is on a campfire that goes out loses its progress two at a time rather than all at once. Nothing
/// is taken out by hand: when a slice is done it drops beside the fire.
namespace campfires {

/// parity: `CampfireBlockEntity`'s four slots, one per side of the fire.
inline constexpr std::int32_t kSize = 4;

[[nodiscard]] const wire::common::Identifier& block();

/// What the caller's recipe book makes of a stack on a campfire, or nothing when it makes nothing.
using ResultFor = std::function<const server::world::crafting::Recipe*(const server::world::loot::ItemStack&)>;

/// What is on the fire, and how far along each piece is.
struct State {
    blockInventory::Slots items = blockInventory::Slots(kSize);
    std::array<std::int32_t, kSize> progress{};
    std::array<std::int32_t, kSize> total{};
};

/// One turn. Answers the slots that finished, so the caller can drop them; whether anything moved is
/// the state having changed, which the caller writes back.
[[nodiscard]] std::vector<server::world::loot::ItemStack> tick(State& state, bool lit, const ResultFor& resultFor);

/// parity: `placeFood`: the first free slot takes it, at the recipe's own cooking time, and food
/// with no campfire recipe is refused rather than left sitting there.
[[nodiscard]] bool placeFood(State& state, const server::world::loot::ItemStack& stack, const ResultFor& resultFor);

/// parity: `CampfireBlock.LIT`, and a soul campfire cooks the same as an ordinary one.
[[nodiscard]] bool isLitCampfire(std::int32_t stateId);

[[nodiscard]] bool isCampfire(std::string_view path);

[[nodiscard]] State read(const wire::nbt::NbtCompound& data);

void write(const State& state, wire::nbt::NbtCompound& data);

} // namespace campfires

} // namespace basalt::server::runtime::world
