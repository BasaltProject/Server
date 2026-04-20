#pragma once

#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace basalt::server::runtime::world {

/// parity: `ShulkerBoxBlock.getDrops`.
namespace shulkers {

inline constexpr std::int32_t kSize = 27;

inline constexpr std::string_view kContainer = "minecraft:container";

[[nodiscard]] std::span<const std::string_view> blocks();

[[nodiscard]] bool isShulkerBox(std::string_view path);

[[nodiscard]] std::optional<wire::nbt::NbtCompound> contentsOf(const wire::nbt::NbtCompound& data);

void restore(wire::nbt::NbtCompound& data, const wire::nbt::NbtCompound& carried);

} // namespace shulkers

} // namespace basalt::server::runtime::world
