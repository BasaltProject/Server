#pragma once

#include <cstdint>

namespace basalt::server::registry::blockFluids {

/// Whether this state exposes a non-empty water fluid state. parity:
/// `BlockBehaviour.BlockStateBase.getFluidState`: almost every case is the `waterlogged` property,
/// the exceptions being the four blocks that are in water without saying so.
///
/// Checked against the hand-written arithmetic it replaces: of 32,366 states the two agreed on all
/// but 30, which are exactly seagrass, tall seagrass, kelp and the kelp plant.
[[nodiscard]] bool hasWaterFluid(std::int32_t stateId);

/// Whether this state exposes any non-empty fluid state, water or lava.
[[nodiscard]] bool hasFluid(std::int32_t stateId);

} // namespace basalt::server::registry::blockFluids
