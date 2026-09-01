#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/structure/MineshaftFill.hpp>
#include <basalt/server/world/structure/StructurePlacementGrid.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <string>
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
    MineshaftStructureGenerator(BiomeAt biomeAt, std::set<std::string, std::less<>> normalBiomes,
                                std::set<std::string, std::less<>> mesaBiomes, std::int32_t seaLevel = 63);

    /// Writes into `chunk` whatever any nearby mineshaft puts there.
    void generateInto(Chunk& chunk, std::int64_t seed);

private:
    /// Which mineshaft, if any, starts at this chunk. parity: `ChunkGenerator` picks by weight from
    /// the set and retries the other entry when the first one's biome refuses it, so a chunk with a
    /// badlands biome still gets a shaft when the normal one was drawn first and rejected.
    [[nodiscard]] std::optional<structure::MineshaftType> typeAt(std::int64_t seed, std::int32_t chunkX,
                                                                 std::int32_t chunkZ) const;

    [[nodiscard]] bool allows(structure::MineshaftType type, const std::string& biome) const;

    /// Assembling a shaft is expensive and every chunk it touches asks for the same one, so the
    /// result is shared behind a lock.
    [[nodiscard]] const std::vector<structure::MineshaftPiece>&
    piecesOf(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ, structure::MineshaftType type);

    BiomeAt biomeAt_;
    std::set<std::string, std::less<>> normalBiomes_;
    std::set<std::string, std::less<>> mesaBiomes_;
    std::int32_t seaLevel_;
    structure::StructurePlacementGrid grid_;
    mutable std::mutex mutex_;
    std::map<std::int64_t, std::vector<structure::MineshaftPiece>> assembled_;
};

} // namespace basalt::server::world::terrain
