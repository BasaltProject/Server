#pragma once

#include <cstdint>
#include <utility>
#include <vector>

namespace basalt::server::world::terrain {

/// The order a chunk's structure references come back in. parity: vanilla keeps them in a
[[nodiscard]] std::vector<std::pair<std::int32_t, std::int32_t>>
vanillaStructureReferenceChunkOrder(const std::vector<std::pair<std::int32_t, std::int32_t>>& references);

} // namespace basalt::server::world::terrain
