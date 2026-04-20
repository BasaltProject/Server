#pragma once

#include <cstdint>
#include <unordered_set>
#include <vector>

namespace basalt::server::runtime::sched {

namespace regionMap {

inline constexpr std::int32_t kGridExponent = 4;

[[nodiscard]] constexpr std::int32_t sectionOf(std::int32_t chunkCoordinate) {
    return chunkCoordinate >> kGridExponent;
}

[[nodiscard]] constexpr std::int64_t sectionKey(std::int32_t sectionX, std::int32_t sectionZ) {
    return (static_cast<std::int64_t>(sectionX) << 32) | (static_cast<std::int64_t>(sectionZ) & 0xFFFFFFFFLL);
}

[[nodiscard]] constexpr std::int32_t sectionX(std::int64_t key) { return static_cast<std::int32_t>(key >> 32); }

[[nodiscard]] constexpr std::int32_t sectionZ(std::int64_t key) { return static_cast<std::int32_t>(key); }

[[nodiscard]] constexpr std::int64_t sectionKeyOfChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    return sectionKey(sectionOf(chunkX), sectionOf(chunkZ));
}

[[nodiscard]] std::vector<std::unordered_set<std::int64_t>>
components(const std::unordered_set<std::int64_t>& sections);

} // namespace regionMap

} // namespace basalt::server::runtime::sched
