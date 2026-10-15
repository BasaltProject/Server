#pragma once

#include <cstdint>

namespace basalt::server::world {

/// parity: `NoiseSettings.OVERWORLD_NOISE_SETTINGS` (`create(-64.
namespace worldHeight {

inline constexpr std::int32_t kMinY = -64;
inline constexpr std::int32_t kHeight = 384;
inline constexpr std::int32_t kMaxY = kMinY + kHeight - 1;
inline constexpr std::int32_t kMinSection = kMinY >> 4;
inline constexpr std::int32_t kSectionCount = kHeight / 16;

} // namespace worldHeight

} // namespace basalt::server::world
