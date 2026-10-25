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

/// parity: `CampfireBlockEntity.cookTick` while it is lit and `cooldownTick` while it is not.
namespace campfires {

inline constexpr std::int32_t kSize = 4;

[[nodiscard]] const wire::common::Identifier& block();

using ResultFor = std::function<const server::world::crafting::Recipe*(const server::world::loot::ItemStack&)>;

struct State {
    blockInventory::Slots items = blockInventory::Slots(kSize);
    std::array<std::int32_t, kSize> progress{};
    std::array<std::int32_t, kSize> total{};
    std::array<bool, kSize> stopped{};
};

[[nodiscard]] std::vector<server::world::loot::ItemStack> tick(State& state, bool lit, const ResultFor& resultFor);

[[nodiscard]] bool placeFood(State& state, const server::world::loot::ItemStack& stack, const ResultFor& resultFor);

[[nodiscard]] bool isLitCampfire(std::int32_t stateId);

[[nodiscard]] bool isCampfire(std::string_view path);

[[nodiscard]] State read(const wire::nbt::NbtCompound& data);

void write(const State& state, wire::nbt::NbtCompound& data);

} // namespace campfires

} // namespace basalt::server::runtime::world
