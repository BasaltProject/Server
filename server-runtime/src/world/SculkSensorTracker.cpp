#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/SculkSensorTracker.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace basalt::server::runtime::world {
namespace {

/// The same packing the chunk map uses: twenty-six bits of x and z around twelve of y.
[[nodiscard]] std::int64_t blockKey(std::int32_t x, std::int32_t y, std::int32_t z) {
    return ((static_cast<std::int64_t>(x) & 0x3ffffff) << 38) | ((static_cast<std::int64_t>(z) & 0x3ffffff) << 12) |
           (static_cast<std::int64_t>(y) & 0xfff);
}

[[nodiscard]] std::int32_t unpackBlockX(std::int64_t packed) { return static_cast<std::int32_t>(packed >> 38); }

[[nodiscard]] std::int32_t unpackBlockY(std::int64_t packed) { return static_cast<std::int32_t>(packed << 52 >> 52); }

[[nodiscard]] std::int32_t unpackBlockZ(std::int64_t packed) { return static_cast<std::int32_t>(packed << 26 >> 38); }

} // namespace

void SculkSensorTracker::remember(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    const std::int64_t position = blockKey(x, y, z);
    const bool sensor =
        block != nullptr && (block->path() == "sculk_sensor" || block->path() == "calibrated_sculk_sensor");
    const auto at = std::ranges::find(sculkSensors_, position);
    if (sensor) {
        if (at == sculkSensors_.end()) {
            sculkSensors_.push_back(position);
        }
    } else if (at != sculkSensors_.end()) {
        sculkSensors_.erase(at);
    }
}

bool SculkSensorTracker::hasNear(double x, double y, double z, std::int32_t range) const {
    // Asked before the lock, as the frozen Kotlin did: on a server where nothing has touched a
    // sensor this is every footstep of every player, and the answer is always the same one.
    if (sculkSensors_.empty()) {
        return false;
    }
    const WorldWriteLock lock{*mutex_};
    return std::ranges::any_of(sculkSensors_, [x, y, z, range](std::int64_t position) {
        return std::abs(unpackBlockX(position) - x) <= range && std::abs(unpackBlockY(position) - y) <= range &&
               std::abs(unpackBlockZ(position) - z) <= range;
    });
}

std::size_t SculkSensorTracker::size() const {
    const WorldWriteLock lock{*mutex_};
    return sculkSensors_.size();
}

} // namespace basalt::server::runtime::world
