#pragma once

#include <basalt/server/world/structure/MonumentBrush.hpp>

namespace basalt::server::world::structure {

/// The three pieces outside the lattice: the two wings, an elder guardian each, and the roof
/// penthouse with the third. parity: `OceanMonumentWingRoom` and `OceanMonumentPenthouse`.
namespace monumentWings {

/// `design` is the low bit of the single draw both wings share, so the two are never the same.
void wing(const MonumentBrush& brush, std::int32_t design);

/// parity: `OceanMonumentPenthouse`. Its floor is one course below the piece's own origin.
void penthouse(const MonumentBrush& brush);

} // namespace monumentWings

} // namespace basalt::server::world::structure
