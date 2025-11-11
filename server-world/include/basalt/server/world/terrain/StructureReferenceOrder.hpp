#pragma once

#include <cstdint>
#include <utility>
#include <vector>

namespace basalt::server::world::terrain {

/// The order a chunk's structure references come back in. parity: vanilla keeps them in a
/// `Long2ObjectOpenHashMap` keyed by the packed chunk position and iterates it backwards, so which
/// start is written first is decided by that map's hash and not by distance or insertion order.
///
/// Refuses more than 23 references: the default table would rehash and the slots would move.
[[nodiscard]] std::vector<std::pair<std::int32_t, std::int32_t>>
vanillaStructureReferenceChunkOrder(const std::vector<std::pair<std::int32_t, std::int32_t>>& references);

} // namespace basalt::server::world::terrain
