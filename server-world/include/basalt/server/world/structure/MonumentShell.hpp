#pragma once

#include <basalt/server/world/structure/MonumentBrush.hpp>

#include <cstdint>

namespace basalt::server::world::structure {

/// parity: `MonumentBuilding.postProcess`.
namespace monumentShell {

void place(const MonumentBrush& brush, std::int32_t seaLevel);

} // namespace monumentShell

} // namespace basalt::server::world::structure
