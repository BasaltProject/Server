#pragma once

#include <cstdint>

namespace basalt::server::world::terrain::decoration {

/// The support a plant needs, rechecked at delivery. A feature writes plant and ground in one go, but
/// across a chunk edge the ground write can be refused while the plant's is accepted, leaving a plant
/// on whatever the receiving chunk put there. This is the feature's own test, applied where it lands.
namespace vegetationBlockRules {

/// Whether `generatedStateId` still stands on `belowStateId`. True for anything that is not a plant.
/// parity: `BushBlock.mayPlaceOn`: the `minecraft:dirt` tag plus farmland.
[[nodiscard]] bool bushMayPlaceOn(std::int32_t belowStateId);

[[nodiscard]] bool survivesDeferred(std::int32_t generatedStateId, std::int32_t belowStateId);

} // namespace vegetationBlockRules

} // namespace basalt::server::world::terrain::decoration
