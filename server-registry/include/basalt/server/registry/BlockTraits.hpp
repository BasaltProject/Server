#pragma once

#include <cstdint>

namespace basalt::server::registry {
// parity: the per-block flags of `BlockBehaviour.BlockStateBase` that no state property changes.
namespace blockTraits {
[[nodiscard]] bool ignitedByLava(std::int32_t stateId);

[[nodiscard]] bool isSignalSource(std::int32_t stateId);

[[nodiscard]] bool hasAnalogOutputSignal(std::int32_t stateId);
} // namespace blockTraits
} // namespace basalt::server::registry
