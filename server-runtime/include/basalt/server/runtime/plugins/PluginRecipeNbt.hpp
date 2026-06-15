#pragma once

#include <basalt/server/world/crafting/Crafting.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <optional>

namespace basalt::server::runtime::plugins {

/// A recipe as a compound that mirrors the struct, which is the form plugins make and read recipes in.
[[nodiscard]] wire::nbt::NbtCompound recipeToNbt(const server::world::crafting::Recipe& recipe);

[[nodiscard]] std::optional<server::world::crafting::Recipe> recipeFromNbt(const wire::nbt::NbtCompound& data);

} // namespace basalt::server::runtime::plugins
