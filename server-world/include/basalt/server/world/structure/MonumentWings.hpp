#pragma once

#include <basalt/server/world/structure/MonumentBrush.hpp>

#include <cstdint>

namespace basalt::server::world::structure {

/// penthouse with the third. parity: `OceanMonumentWingRoom` and `OceanMonumentPenthouse`.
namespace monumentWings {

void wing(const MonumentBrush& brush, std::int32_t design);

/// parity: `OceanMonumentPenthouse`.
void penthouse(const MonumentBrush& brush);

} // namespace monumentWings

} // namespace basalt::server::world::structure
