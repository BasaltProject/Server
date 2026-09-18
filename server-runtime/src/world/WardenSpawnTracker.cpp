#include <basalt/server/runtime/world/WardenSpawnTracker.hpp>

#include <algorithm>

namespace basalt::server::runtime::world::wardenSpawnTracker {

void Tracker::tick() {
    if (ticksSinceLastWarning >= kWarningDecayTicks) {
        warningLevel = std::clamp(warningLevel - 1, 0, kMaxWarning);
        ticksSinceLastWarning = 0;
    } else {
        ticksSinceLastWarning++;
    }
    if (cooldownTicks > 0) {
        cooldownTicks--;
    }
}

std::optional<std::int32_t> warn(const std::vector<Tracker*>& trackers) {
    if (trackers.empty() ||
        std::ranges::any_of(trackers, [](const Tracker* tracker) { return tracker->cooldownTicks > 0; })) {
        return std::nullopt;
    }
    // parity: `Stream.max`, which keeps the last of equals.
    Tracker* highest = trackers.front();
    for (Tracker* tracker : trackers) {
        if (tracker->warningLevel >= highest->warningLevel) {
            highest = tracker;
        }
    }
    highest->ticksSinceLastWarning = 0;
    highest->cooldownTicks = kWarningCooldown;
    highest->warningLevel = std::clamp(highest->warningLevel + 1, 0, kMaxWarning);
    const Tracker copy = *highest;
    for (Tracker* tracker : trackers) {
        *tracker = copy;
    }
    return copy.warningLevel;
}

} // namespace basalt::server::runtime::world::wardenSpawnTracker
