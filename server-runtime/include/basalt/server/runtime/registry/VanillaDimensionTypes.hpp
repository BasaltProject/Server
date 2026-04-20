#pragma once

#include <basalt/wire/nbt/NbtTag.hpp>

#include <string>
#include <utility>
#include <vector>

namespace basalt::server::runtime::registry {

namespace vanillaDimensionTypes {

[[nodiscard]] const std::vector<std::pair<std::string, wire::nbt::NbtCompound>>& worldClocks();

[[nodiscard]] const std::vector<std::pair<std::string, wire::nbt::NbtCompound>>& dimensionTypes();

[[nodiscard]] wire::nbt::NbtCompound overworld();

} // namespace vanillaDimensionTypes

} // namespace basalt::server::runtime::registry
