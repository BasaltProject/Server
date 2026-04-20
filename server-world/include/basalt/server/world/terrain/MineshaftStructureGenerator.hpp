#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/structure/MineshaftFill.hpp>
#include <basalt/server/world/structure/MineshaftLayout.hpp>
#include <basalt/server/world/structure/StructurePlacementGrid.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace basalt::server::world::terrain {

/// Mineshafts, from which chunks start one to which blocks land in this chunk. parity: the set is
class MineshaftStructureGenerator {
public:
    using BiomeAt = std::function<std::string(std::int32_t, std::int32_t, std::int32_t)>;

    MineshaftStructureGenerator(BiomeAt biomeAt, std::set<std::string, std::less<>> normalBiomes,
                                std::set<std::string, std::less<>> mesaBiomes,
                                structure::mineshaftLayout::SurfaceHeightAt surfaceHeightAt = {},
                                std::int32_t seaLevel = 63, BiomeAt startBiomeAt = {});

    void generateInto(Chunk& chunk, std::int64_t seed);

    /// parity: `PlaceCommand.placeStructure`.
    [[nodiscard]] std::optional<structure::StructureBoundingBox>
    placeBoundsAt(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ, structure::MineshaftType type) const;

    void placeAt(Chunk& chunk, std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ,
                 structure::MineshaftType type) const;

    [[nodiscard]] const structure::StructurePlacementGrid& grid() const noexcept { return grid_; }

    /// Which mineshaft, if any, starts at this chunk. parity: `ChunkGenerator` picks by weight from
    [[nodiscard]] std::optional<structure::MineshaftType> typeAt(std::int64_t seed, std::int32_t chunkX,
                                                                 std::int32_t chunkZ) const;

private:
    [[nodiscard]] bool allows(structure::MineshaftType type, const std::string& biome) const;

    [[nodiscard]] bool accepts(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ,
                               structure::MineshaftType type) const;

    [[nodiscard]] std::shared_ptr<const structure::mineshaftLayout::Assembly>
    assemblyOf(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ, structure::MineshaftType type) const;

    BiomeAt biomeAt_;
    /// parity: `Structure.isValidBiome` reads the noise biome at the stub.
    BiomeAt startBiomeAt_;
    std::set<std::string, std::less<>> normalBiomes_;
    std::set<std::string, std::less<>> mesaBiomes_;
    structure::mineshaftLayout::SurfaceHeightAt surfaceHeightAt_;
    std::int32_t seaLevel_;
    structure::StructurePlacementGrid grid_;
    mutable std::mutex mutex_;
    mutable std::map<std::pair<std::int64_t, structure::MineshaftType>,
                     std::shared_ptr<const structure::mineshaftLayout::Assembly>>
        assembled_;
};

} // namespace basalt::server::world::terrain
