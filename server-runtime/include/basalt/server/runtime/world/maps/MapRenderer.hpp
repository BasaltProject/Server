#pragma once

#include <basalt/server/runtime/world/maps/MapData.hpp>

#include <cstdint>
#include <functional>
#include <optional>

namespace basalt::server::world {
class Chunk;
}

namespace basalt::server::runtime::world::maps {

/// What the colour scan reads of the world a map is drawn in.
struct ScanWorld {
    /// The chunk, or null when it is not loaded.
    std::function<const server::world::Chunk*(std::int32_t chunkX, std::int32_t chunkZ)> chunkAt;
    std::function<std::optional<MapBanner>(const BlockPos&)> bannerAt;
    std::int32_t minY = 0;
    bool hasCeiling = false;
};

namespace mapRenderer {

/// parity: `MapItem.update`, one step of the scan around a player holding the map. A chunk that is
/// not loaded is skipped, where vanilla loads it.
void update(MapData& data, MapData::Holder& holder, const ScanWorld& world, double playerX, double playerZ);

/// parity: `MapItem.renderBiomePreviewMap`. `watery` answers whether the biome at a block position is
/// in `water_on_map_outlines`.
void renderBiomePreview(MapData& data, std::int32_t seaLevel,
                        const std::function<bool(std::int32_t x, std::int32_t y, std::int32_t z)>& watery);

} // namespace mapRenderer

} // namespace basalt::server::runtime::world::maps
