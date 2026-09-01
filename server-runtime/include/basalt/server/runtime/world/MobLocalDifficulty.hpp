#pragma once

#include <cstdint>
#include <string_view>

namespace basalt::server::runtime::world {

/// Minecraft's local difficulty multiplier, which is what mob finalizers ask before deciding how
/// nasty a spawn is.
namespace mobLocalDifficulty {

/// parity: `DifficultyInstance.getSpecialMultiplier`: nothing below two, everything above four, and
/// a straight line between them.
[[nodiscard]] float specialMultiplier(std::string_view difficulty, std::int64_t gameTime, std::int64_t inhabitedTime,
                                      float moonBrightness);

/// parity: `DifficultyInstance.getEffectiveDifficulty`, also used by the custom spawners.
[[nodiscard]] float effectiveDifficulty(std::string_view difficulty, std::int64_t gameTime, std::int64_t inhabitedTime,
                                        float moonBrightness);

} // namespace mobLocalDifficulty

} // namespace basalt::server::runtime::world
