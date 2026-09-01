#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/structure/StructurePlacementGrid.hpp>
#include <basalt/server/world/structure/StructureTransform.hpp>

#include <cstdint>
#include <functional>
#include <string>

namespace basalt::server::world::terrain {

/// The swamp hut, vanilla's one-piece scattered structure. parity: `SwampHutStructure`. The piece is
/// 7 by 9 from the chunk's own minimum corner, so it never leaves its chunk whichever way it faces.
class SwampHutStructureGenerator {
public:
    using BiomeAt = std::function<std::string(std::int32_t, std::int32_t, std::int32_t)>;
    /// The noise column's own surface, not the chunk heightmap: the carvers have already run.
    using SurfaceHeightAt = std::function<std::int32_t(std::int32_t, std::int32_t)>;

    static constexpr std::string_view kName = "swamp_hut";
    /// Twenty-first of the 26 structures of `surface_structures`, in registry order.
    static constexpr std::int32_t kStructureIndex = 20;
    static constexpr std::int32_t kSurfaceStructuresStep = 4;

    SwampHutStructureGenerator(BiomeAt biomeAt, SurfaceHeightAt surfaceHeightAt);

    void generateInto(Chunk& chunk, std::int64_t seed);

    [[nodiscard]] static const structure::StructurePlacementGrid& grid();

private:
    BiomeAt biomeAt_;
    SurfaceHeightAt surfaceHeightAt_;
};

} // namespace basalt::server::world::terrain
