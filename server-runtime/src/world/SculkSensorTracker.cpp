#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/SculkSensorTracker.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace basalt::server::runtime::world {
namespace {

[[nodiscard]] std::int64_t blockKey(std::int32_t x, std::int32_t y, std::int32_t z) {
    return ((static_cast<std::int64_t>(x) & 0x3ffffff) << 38) | ((static_cast<std::int64_t>(z) & 0x3ffffff) << 12) |
           (static_cast<std::int64_t>(y) & 0xfff);
}

[[nodiscard]] std::int32_t unpackBlockX(std::int64_t packed) { return static_cast<std::int32_t>(packed >> 38); }

[[nodiscard]] std::int32_t unpackBlockY(std::int64_t packed) { return static_cast<std::int32_t>(packed << 52 >> 52); }

[[nodiscard]] std::int32_t unpackBlockZ(std::int64_t packed) { return static_cast<std::int32_t>(packed << 26 >> 38); }

/// parity: the two blocks that listen. A vein of sculk in the deep dark writes them straight into.
[[nodiscard]] bool isSensorState(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block != nullptr && (block->path() == "sculk_sensor" || block->path() == "calibrated_sculk_sensor");
}

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

void SculkSensorTracker::rememberChunk(const server::world::Chunk& chunk) {
    const std::int32_t baseX = chunk.chunkX() << 4;
    const std::int32_t baseZ = chunk.chunkZ() << 4;
    const auto keep = [this](std::int32_t x, std::int32_t y, std::int32_t z) {
        const std::int64_t position = blockKey(x, y, z);
        if (std::ranges::find(sculkSensors_, position) == sculkSensors_.end()) {
            sculkSensors_.push_back(position);
        }
    };
    for (const server::world::ChunkBlockEntity& entity : chunk.blockEntities()) {
        const std::string_view path = entity.type().path();
        if (path == "sculk_sensor" || path == "calibrated_sculk_sensor") {
            keep(baseX + entity.localX(), entity.y(), baseZ + entity.localZ());
        }
    }
    const std::int32_t minY = chunk.verticalBounds().minY();
    for (std::size_t index = 0; index < chunk.sectionCount(); ++index) {
        const server::world::ChunkSection& section = chunk.section(index);
        if (!section.mayContain([](std::int32_t stateId) { return isSensorState(stateId); })) {
            continue;
        }
        const std::int32_t baseY = minY + (static_cast<std::int32_t>(index) * 16);
        for (std::int32_t localY = 0; localY < 16; ++localY) {
            for (std::int32_t localZ = 0; localZ < 16; ++localZ) {
                for (std::int32_t localX = 0; localX < 16; ++localX) {
                    if (isSensorState(section.blockStateId(localX, localY, localZ))) {
                        keep(baseX + localX, baseY + localY, baseZ + localZ);
                    }
                }
            }
        }
    }
}

void SculkSensorTracker::forgetChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    const auto gone = std::ranges::remove_if(sculkSensors_, [chunkX, chunkZ](std::int64_t position) {
        return (unpackBlockX(position) >> 4) == chunkX && (unpackBlockZ(position) >> 4) == chunkZ;
    });
    sculkSensors_.erase(gone.begin(), gone.end());
}

bool SculkSensorTracker::hasNear(double x, double y, double z, std::int32_t range) const {
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
