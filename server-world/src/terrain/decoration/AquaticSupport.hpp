#pragma once

#include <cstdint>

namespace basalt::server::world::terrain::decoration::aquaticsupport {

/// parity: `Seagrass.canSurvive`, which refuses magma and wants a sturdy top face.
[[nodiscard]] bool seagrassSupport(std::int32_t below);

/// parity: `SeaPickleBlock.canSurvive`.
[[nodiscard]] bool seaPickleSupport(std::int32_t below);

/// parity: `KelpBlock.canSurvive`, which grows on its own stalk as well as on the floor.
[[nodiscard]] bool kelpSupport(std::int32_t below);

} // namespace basalt::server::world::terrain::decoration::aquaticsupport
