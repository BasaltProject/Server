#pragma once

#include <basalt/server/runtime/world/maps/MapData.hpp>

#include <cstdint>
#include <functional>
#include <optional>

namespace basalt::server::world {
class Chunk;
}

namespace basalt::server::runtime::world::maps {

struct ScanWorld {
    std::function<const server::world::Chunk*(std::int32_t chunkX, std::int32_t chunkZ)> chunkAt;
    std::function<std::optional<MapBanner>(const BlockPos&)> bannerAt;
    std::int32_t minY = 0;
    bool hasCeiling = false;
};

namespace mapRenderer {

/// parity: `MapItem.update`.
void update(MapData& data, MapData::Holder& holder, const ScanWorld& world, double playerX, double playerZ);

/// parity: `MapItem.renderBiomePreviewMap`.
void renderBiomePreview(MapData& data, std::int32_t seaLevel,
                        const std::function<bool(std::int32_t x, std::int32_t y, std::int32_t z)>& watery);

} // namespace mapRenderer

} // namespace basalt::server::runtime::world::maps
