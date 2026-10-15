#pragma once

#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <functional>

namespace basalt::server::runtime::world {

class LoadedWorld;

namespace spawnPlaces {

/// parity: the moon phase as `Level.getMoonBrightness` answers it.
[[nodiscard]] float moonBrightness(const LoadedWorld& world);

/// parity: `StructureManager.getStructureWithPieceAt`.
[[nodiscard]] std::function<bool(const wire::common::Identifier&, std::int32_t, std::int32_t, std::int32_t)>
structureAt(LoadedWorld& world);

} // namespace spawnPlaces

} // namespace basalt::server::runtime::world
