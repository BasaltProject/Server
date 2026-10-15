#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace basalt::server::runtime::world {

namespace wardenSpawnTracker {

inline constexpr std::int32_t kMaxWarning = 4;
/// parity: ten minutes a level and ten seconds between warnings.
inline constexpr std::int32_t kWarningDecayTicks = 12000;
inline constexpr std::int32_t kWarningCooldown = 200;

struct Tracker {
    std::int32_t ticksSinceLastWarning = 0;
    std::int32_t warningLevel = 0;
    std::int32_t cooldownTicks = 0;

    void tick();

    friend bool operator==(const Tracker&, const Tracker&) = default;
};

/// parity: `tryWarn`.
[[nodiscard]] std::optional<std::int32_t> warn(const std::vector<Tracker*>& trackers);

} // namespace wardenSpawnTracker

} // namespace basalt::server::runtime::world
