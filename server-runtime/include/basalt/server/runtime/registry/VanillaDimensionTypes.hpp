#pragma once

#include <basalt/wire/nbt/NbtTag.hpp>

#include <string>
#include <utility>
#include <vector>

namespace basalt::server::runtime::registry {

/// Hand-authored NBT for the dimension registries a real 26.2 client needs during Configuration.
///
/// Field shapes come from decompiling the real 26.2 server jar's codecs and bootstrap data, not
/// guessed from memory: the schemas changed substantially from older, widely-known versions.
namespace vanillaDimensionTypes {

/// parity: `WorldClocks.bootstrap`; both clocks use the empty default clock codec.
[[nodiscard]] const std::vector<std::pair<std::string, wire::nbt::NbtCompound>>& worldClocks();

/// parity: `DimensionTypes.bootstrap`, in its exact registry order.
[[nodiscard]] const std::vector<std::pair<std::string, wire::nbt::NbtCompound>>& dimensionTypes();

/// The overworld's own entry, which is what a world of that kind is joined with.
[[nodiscard]] wire::nbt::NbtCompound overworld();

} // namespace vanillaDimensionTypes

} // namespace basalt::server::runtime::registry
