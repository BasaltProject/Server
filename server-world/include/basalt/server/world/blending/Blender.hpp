#pragma once

#include <basalt/server/world/blending/BlendingData.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace basalt::server::world::blending {

/// parity: `Blender`, the terrain, biome and carving of a new chunk bent towards the old chunks around it.
class Blender {
public:
    struct Source {
        std::int32_t chunkX;
        std::int32_t chunkZ;
        std::shared_ptr<const BlendingData> data;
    };

    struct Output {
        float alpha;
        float offset;
    };

    using CarvingFilter = std::function<bool(std::int32_t x, std::int32_t y, std::int32_t z)>;

    /// `heightAndBiome` and `density` come in the order vanilla inserts them; their walk follows fastutil's table.
    Blender(std::vector<Source> heightAndBiome, std::vector<Source> density, CarvingFilter carvingFilter);

    [[nodiscard]] bool isEmpty() const noexcept {
        return heightAndBiome_.empty() && density_.empty() && !carvingFilter_;
    }

    [[nodiscard]] Output blendOffsetAndFactor(std::int32_t blockX, std::int32_t blockZ) const;

    [[nodiscard]] float blendDensity(std::int32_t blockX, std::int32_t blockY, std::int32_t blockZ,
                                     float noiseValue) const;

    [[nodiscard]] std::optional<std::int32_t> blendBiome(std::int32_t quartX, std::int32_t quartY,
                                                         std::int32_t quartZ) const;

    [[nodiscard]] const CarvingFilter& carvingFilter() const noexcept { return carvingFilter_; }

    /// parity: `Blender.makeOldChunkDistanceGetter`, the distance from a point to the nearest old chunk's cube.
    struct OldSide {
        std::optional<Side> side;
        std::shared_ptr<const BlendingData> data;
    };

    [[nodiscard]] static CarvingFilter makeCarvingFilter(std::vector<OldSide> sides);

private:
    std::vector<Source> heightAndBiome_;
    std::vector<Source> density_;
    CarvingFilter carvingFilter_;
};

} // namespace basalt::server::world::blending
