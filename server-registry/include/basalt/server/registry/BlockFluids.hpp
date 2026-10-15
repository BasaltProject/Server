#pragma once

#include <cstdint>

namespace basalt::server::registry::blockFluids {
[[nodiscard]] bool hasWaterFluid(std::int32_t stateId);

[[nodiscard]] bool hasFluid(std::int32_t stateId);
} // namespace basalt::server::registry::blockFluids
