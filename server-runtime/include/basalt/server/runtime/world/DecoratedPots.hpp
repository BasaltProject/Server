#pragma once

#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/world/loot/Loot.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// parity: `DecoratedPotBlock`.
namespace decoratedPots {

inline constexpr std::int32_t kSize = 1;

inline constexpr std::string_view kSherds = "minecraft:sherds";

[[nodiscard]] const wire::common::Identifier& block();

[[nodiscard]] bool isPot(std::string_view path);

[[nodiscard]] std::optional<server::world::loot::ItemStack> contents(const wire::nbt::NbtCompound& data);

/// parity: `DecoratedPotBlock.useItemOn`.
[[nodiscard]] std::optional<std::vector<BlockEffect>> insert(wire::nbt::NbtCompound& data,
                                                             const wire::common::Identifier& item,
                                                             const wire::nbt::NbtCompound& components = {});

/// parity: `DecoratedPotBlock.useWithoutItem`: a wobble and a thud, nothing else.
[[nodiscard]] std::vector<BlockEffect> rebuff();

/// parity: 26.3 `getDrops` hands them to the `sherds` dynamic drop.
[[nodiscard]] std::vector<server::world::loot::ItemStack> sherds(const wire::nbt::NbtCompound& data);

/// parity: `DecoratedPotBlockEntity.applyImplicitComponents`.
void decorate(wire::nbt::NbtCompound& data, const wire::nbt::NbtCompound& components);

/// parity: `RandomizableContainer`.
[[nodiscard]] std::optional<server::world::loot::ItemStack> takeOne(wire::nbt::NbtCompound& data);

} // namespace decoratedPots

} // namespace basalt::server::runtime::world
