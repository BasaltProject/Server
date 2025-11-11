#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/structure/StructurePlacementGrid.hpp>
#include <basalt/server/world/terrain/JungleTemplePiece.hpp>

#include <cstdint>
#include <functional>
#include <set>
#include <string>

namespace basalt::server::world::terrain {

/// The jungle temple. parity: `JungleTempleStructure`, one piece twelve by fifteen, so the chunks it
/// reaches into place their own part of it.
class JungleTempleStructureGenerator {
public:
    using BiomeAt = std::function<std::string(std::int32_t, std::int32_t, std::int32_t)>;
    using SurfaceHeightAt = std::function<std::int32_t(std::int32_t, std::int32_t)>;

    static constexpr std::string_view kName = "jungle_pyramid";
    /// Fifth of the 26 structures of `surface_structures`, in registry order.
    static constexpr std::int32_t kStructureIndex = 4;

    JungleTempleStructureGenerator(BiomeAt biomeAt, std::set<std::string, std::less<>> templeBiomes,
                                   SurfaceHeightAt surfaceHeightAt);

    void generateInto(Chunk& chunk, std::int64_t seed);

    [[nodiscard]] static const structure::StructurePlacementGrid& grid();

private:
    void placeFrom(Chunk& chunk, std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ) const;

    BiomeAt biomeAt_;
    std::set<std::string, std::less<>> templeBiomes_;
    SurfaceHeightAt surfaceHeightAt_;
};

} // namespace basalt::server::world::terrain
