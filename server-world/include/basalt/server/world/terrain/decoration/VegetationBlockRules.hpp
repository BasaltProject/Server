#pragma once

#include <cstdint>

namespace basalt::server::world::terrain::decoration {

namespace vegetationBlockRules {

/// parity: `BushBlock.mayPlaceOn`: the `minecraft:dirt` tag plus farmland.
[[nodiscard]] bool bushMayPlaceOn(std::int32_t belowStateId);

[[nodiscard]] bool survivesDeferred(std::int32_t generatedStateId, std::int32_t belowStateId);

} // namespace vegetationBlockRules

} // namespace basalt::server::world::terrain::decoration
