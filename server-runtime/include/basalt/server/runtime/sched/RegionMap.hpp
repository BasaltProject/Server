#pragma once

#include <cstdint>
#include <unordered_set>
#include <vector>

namespace basalt::server::runtime::sched {

/// How the loaded chunks of a world are grouped into regions.
///
/// parity of model with `ThreadedRegionizer`: the world is cut into a grid of sections, and a
/// region is a set of sections that touch each other. Two chunks that can interact inside one tick
/// have to end up in the same region, which is why the grid is coarse (sixteen chunks a side,
/// Folia's own `gridExponent = 4`) and why sections that touch diagonally count as connected.
///
/// Pure on purpose: it takes the sections that have chunks in them and answers the groups. Who owns
/// what, what to do when two groups become one and what happens to the work already queued are
/// `RegionScheduler`'s, and are the part that has to run at a tick boundary.
namespace regionMap {

/// parity: Folia's `gridExponent`, which makes a section sixteen chunks a side.
inline constexpr std::int32_t kGridExponent = 4;

/// The section a chunk belongs to.
[[nodiscard]] constexpr std::int32_t sectionOf(std::int32_t chunkCoordinate) {
    return chunkCoordinate >> kGridExponent;
}

/// One section as a single number, which is how the sets below are keyed.
[[nodiscard]] constexpr std::int64_t sectionKey(std::int32_t sectionX, std::int32_t sectionZ) {
    return (static_cast<std::int64_t>(sectionX) << 32) | (static_cast<std::int64_t>(sectionZ) & 0xFFFFFFFFLL);
}

[[nodiscard]] constexpr std::int32_t sectionX(std::int64_t key) { return static_cast<std::int32_t>(key >> 32); }

[[nodiscard]] constexpr std::int32_t sectionZ(std::int64_t key) { return static_cast<std::int32_t>(key); }

/// The section key of a chunk, which is what a caller usually has.
[[nodiscard]] constexpr std::int64_t sectionKeyOfChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    return sectionKey(sectionOf(chunkX), sectionOf(chunkZ));
}

/// The groups of sections that touch each other, largest first so the biggest group keeps the
/// region that was already there when several of them merge.
[[nodiscard]] std::vector<std::unordered_set<std::int64_t>>
components(const std::unordered_set<std::int64_t>& sections);

} // namespace regionMap

} // namespace basalt::server::runtime::sched
