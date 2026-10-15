#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/structure/StructurePlacementGrid.hpp>
#include <basalt/server/world/terrain/JungleTemplePiece.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <set>
#include <string>

namespace basalt::server::world::terrain {

/// The jungle temple. parity: `JungleTempleStructure`, one piece twelve by fifteen, so the chunks it
class JungleTempleStructureGenerator {
public:
    using BiomeAt = std::function<std::string(std::int32_t, std::int32_t, std::int32_t)>;
    using SurfaceHeightAt = std::function<std::int32_t(std::int32_t, std::int32_t)>;

    static constexpr std::string_view kName = "jungle_pyramid";
    static constexpr std::int32_t kStructureIndex = 22;

    JungleTempleStructureGenerator(BiomeAt biomeAt, std::set<std::string, std::less<>> templeBiomes,
                                   SurfaceHeightAt surfaceHeightAt);

    void generateInto(Chunk& chunk, std::int64_t seed);

    /// parity: `PlaceCommand.placeStructure`.
    [[nodiscard]] std::optional<structure::StructureBoundingBox> placeBoundsAt(std::int64_t seed, std::int32_t chunkX,
                                                                               std::int32_t chunkZ) const;

    void placeAt(Chunk& chunk, std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ) const;

    [[nodiscard]] static const structure::StructurePlacementGrid& grid();

    [[nodiscard]] bool startsAt(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ) const {
        return startPiece(seed, chunkX, chunkZ).has_value();
    }

private:
    [[nodiscard]] std::optional<JungleTemplePiece> startPiece(std::int64_t seed, std::int32_t chunkX,
                                                              std::int32_t chunkZ, bool forPlacement = false) const;

    void placeFrom(Chunk& chunk, std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ,
                   bool forPlacement = false) const;

    BiomeAt biomeAt_;
    std::set<std::string, std::less<>> templeBiomes_;
    SurfaceHeightAt surfaceHeightAt_;
};

} // namespace basalt::server::world::terrain
