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
/// `spacing 1`, `separation 0`, `frequency 0.004`, so the roll is what makes one rare. A shaft
/// reaches 80 blocks, so a chunk looks several chunks out for the starts that can touch it.
class MineshaftStructureGenerator {
public:
    using BiomeAt = std::function<std::string(std::int32_t, std::int32_t, std::int32_t)>;

    /// `normalBiomes` is `#minecraft:has_structure/mineshaft` and `mesaBiomes` is
    /// `#minecraft:has_structure/mineshaft_mesa`, both read from the jar by the caller.
    /// `surfaceHeightAt` is only read by the mesa variant, which is lifted to the surface.
    MineshaftStructureGenerator(BiomeAt biomeAt, std::set<std::string, std::less<>> normalBiomes,
                                std::set<std::string, std::less<>> mesaBiomes,
                                structure::mineshaftLayout::SurfaceHeightAt surfaceHeightAt = {},
                                std::int32_t seaLevel = 63, BiomeAt startBiomeAt = {});

    /// Writes into `chunk` whatever any nearby mineshaft puts there.
    void generateInto(Chunk& chunk, std::int64_t seed);

    [[nodiscard]] const structure::StructurePlacementGrid& grid() const noexcept { return grid_; }

    /// Which mineshaft, if any, starts at this chunk. parity: `ChunkGenerator` picks by weight from
    /// the set and retries the other entry when the first one's biome refuses it, so a chunk with a
    /// badlands biome still gets a shaft when the normal one was drawn first and rejected.
    [[nodiscard]] std::optional<structure::MineshaftType> typeAt(std::int64_t seed, std::int32_t chunkX,
                                                                 std::int32_t chunkZ) const;

private:
    [[nodiscard]] bool allows(structure::MineshaftType type, const std::string& biome) const;

    /// Whether a shaft of this type survives the biome check at its own start.
    [[nodiscard]] bool accepts(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ,
                               structure::MineshaftType type) const;

    /// Assembling a shaft is expensive and every chunk it touches asks for the same one, so the
    /// result is shared behind a lock.
    /// Shared rather than borrowed: the cache is cleared when it fills, and a reference into it
    /// would dangle the moment another worker triggered that while this one was still reading.
    [[nodiscard]] std::shared_ptr<const structure::mineshaftLayout::Assembly>
    assemblyOf(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ, structure::MineshaftType type) const;

    BiomeAt biomeAt_;
    /// parity: `Structure.isValidBiome` reads the noise biome at the stub, while the pieces' own
    /// `mineshaft_blocking` test reads the zoomed one. Unset, the start reads `biomeAt_` too.
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
