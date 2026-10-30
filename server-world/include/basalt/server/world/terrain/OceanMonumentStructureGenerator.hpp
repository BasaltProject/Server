#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/structure/MonumentRoomFitters.hpp>
#include <basalt/server/world/structure/StructurePlacementGrid.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <set>
#include <string>

namespace basalt::server::world::terrain {

/// The ocean monument. parity: `OceanMonumentStructure`, its base is always at y 39.
class OceanMonumentStructureGenerator {
public:
    using NoiseBiomeAt = std::function<std::string(std::int32_t, std::int32_t, std::int32_t)>;
    using OceanFloorAt = std::function<std::int32_t(std::int32_t, std::int32_t)>;

    static constexpr std::string_view kName = "monument";
    static constexpr std::int32_t kStructureIndex = 24;

    OceanMonumentStructureGenerator(NoiseBiomeAt noiseBiomeAt, std::set<std::string, std::less<>> monumentBiomes,
                                    std::set<std::string, std::less<>> surroundingBiomes, OceanFloorAt oceanFloorAt,
                                    std::int32_t seaLevel = 63);

    void generateInto(Chunk& chunk, std::int64_t seed);

    /// parity: `PlaceCommand.placeStructure`.
    [[nodiscard]] std::optional<structure::StructureBoundingBox> placeBoundsAt(std::int64_t seed, std::int32_t chunkX,
                                                                               std::int32_t chunkZ) const;

    void placeAt(Chunk& chunk, std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ);

    [[nodiscard]] static const structure::StructurePlacementGrid& grid();

    [[nodiscard]] bool startsAt(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ) const;

private:
    void placeFrom(Chunk& chunk, std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ,
                   bool forPlacement = false);

    /// parity: the radius-29 sweep around the ninth block of the chunk. It is a cube of biome cells.
    [[nodiscard]] bool surroundingIsOcean(std::int32_t chunkX, std::int32_t chunkZ) const;

    NoiseBiomeAt noiseBiomeAt_;
    std::set<std::string, std::less<>> monumentBiomes_;
    std::set<std::string, std::less<>> surroundingBiomes_;
    OceanFloorAt oceanFloorAt_;
    std::int32_t seaLevel_;
};

} // namespace basalt::server::world::terrain
