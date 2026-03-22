#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/structure/MonumentRoomFitters.hpp>
#include <basalt/server/world/structure/StructurePlacementGrid.hpp>

#include <cstdint>
#include <functional>
#include <set>
#include <string>

namespace basalt::server::world::terrain {

/// The ocean monument. parity: `OceanMonumentStructure`. Its base is always at y 39 whatever the sea
/// bed does, so unlike every other structure here nothing is measured off the ground: the height is
/// only consulted to decide whether the monument may stand at all.
class OceanMonumentStructureGenerator {
public:
    using NoiseBiomeAt = std::function<std::string(std::int32_t, std::int32_t, std::int32_t)>;
    using OceanFloorAt = std::function<std::int32_t(std::int32_t, std::int32_t)>;

    static constexpr std::string_view kName = "monument";
    /// Position among the 44 structures of `surface_structures`, in registry order.
    static constexpr std::int32_t kStructureIndex = 24;

    OceanMonumentStructureGenerator(NoiseBiomeAt noiseBiomeAt, std::set<std::string, std::less<>> monumentBiomes,
                                    std::set<std::string, std::less<>> surroundingBiomes, OceanFloorAt oceanFloorAt,
                                    std::int32_t seaLevel = 63);

    void generateInto(Chunk& chunk, std::int64_t seed);

    [[nodiscard]] static const structure::StructurePlacementGrid& grid();

private:
    void placeFrom(Chunk& chunk, std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ);

    /// parity: the radius-29 sweep around the ninth block of the chunk. It is a cube of biome cells,
    /// not a circle, and one cell outside the tag is enough to refuse the monument.
    [[nodiscard]] bool surroundingIsOcean(std::int32_t chunkX, std::int32_t chunkZ) const;

    NoiseBiomeAt noiseBiomeAt_;
    std::set<std::string, std::less<>> monumentBiomes_;
    std::set<std::string, std::less<>> surroundingBiomes_;
    OceanFloorAt oceanFloorAt_;
    std::int32_t seaLevel_;
};

} // namespace basalt::server::world::terrain
