#pragma once

#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <functional>

namespace basalt::server::runtime::world {

class LoadedWorld;

/// What a spawn asks of the world around it beyond the biome.
namespace spawnPlaces {

/// parity: the moon phase as `Level.getMoonBrightness` answers it, which is the slime curve doubled.
[[nodiscard]] float moonBrightness(const LoadedWorld& world);

/// parity: `StructureManager.getStructureWithPieceAt`, over the loaded chunks' structure starts.
[[nodiscard]] std::function<bool(const wire::common::Identifier&, std::int32_t, std::int32_t, std::int32_t)>
structureAt(LoadedWorld& world);

} // namespace spawnPlaces

} // namespace basalt::server::runtime::world
