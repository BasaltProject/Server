#pragma once

#include <cstdint>

namespace basalt::server::world {

/// Overworld vertical-bound compatibility constants. New dimension-aware code uses `ChunkHeight`.
///
/// parity: `NoiseSettings.OVERWORLD_NOISE_SETTINGS` (`create(-64, 384, 1, 2)`), confirmed directly
/// against the real 26.2 jar, not just assumed stable from older versions.
namespace worldHeight {

inline constexpr std::int32_t kMinY = -64;
inline constexpr std::int32_t kHeight = 384;
inline constexpr std::int32_t kMaxY = kMinY + kHeight - 1;
inline constexpr std::int32_t kMinSection = kMinY >> 4;
inline constexpr std::int32_t kSectionCount = kHeight / 16;

} // namespace worldHeight

} // namespace basalt::server::world
