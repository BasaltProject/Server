#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/structure/StructurePlacementGrid.hpp>
#include <basalt/server/world/structure/StructureTransform.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace basalt::server::world::terrain {

/// The swamp hut, vanilla's one-piece scattered structure. parity: `SwampHutStructure`.
class SwampHutStructureGenerator {
public:
    using BiomeAt = std::function<std::string(std::int32_t, std::int32_t, std::int32_t)>;
    using SurfaceHeightAt = std::function<std::int32_t(std::int32_t, std::int32_t)>;

    static constexpr std::string_view kName = "swamp_hut";
    static constexpr std::int32_t kStructureIndex = 38;
    static constexpr std::int32_t kSurfaceStructuresStep = 4;

    SwampHutStructureGenerator(BiomeAt biomeAt, SurfaceHeightAt surfaceHeightAt);

    void generateInto(Chunk& chunk, std::int64_t seed);

    /// parity: `PlaceCommand.placeStructure`.
    [[nodiscard]] std::optional<structure::StructureBoundingBox> placeBoundsAt(std::int64_t seed, std::int32_t chunkX,
                                                                               std::int32_t chunkZ) const;

    void placeAt(Chunk& chunk, std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ);

    [[nodiscard]] static const structure::StructurePlacementGrid& grid();

    [[nodiscard]] bool startsAt(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ) const;

private:
    void placeHut(Chunk& chunk, std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ, bool forPlacement = false);

    BiomeAt biomeAt_;
    SurfaceHeightAt surfaceHeightAt_;
};

} // namespace basalt::server::world::terrain
