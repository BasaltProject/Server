#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/densityfunction/DensityFunction.hpp>
#include <basalt/server/world/structure/ConcentricRingsPlacement.hpp>
#include <basalt/server/world/structure/StrongholdLayout.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace basalt::server::world::terrain {

/// The stronghold. parity: `StrongholdStructure`. Unlike every other structure here it is not spread
/// over a grid: the world holds 128 fixed chunks laid out on rings, and the maze itself reaches 112
/// blocks out from the chunk that starts it.
class StrongholdStructureGenerator {
public:
    using NoiseBiomeAt = std::function<std::string(std::int32_t, std::int32_t, std::int32_t)>;

    static constexpr std::string_view kName = "stronghold";
    /// Twentieth of the 26 structures of `surface_structures`, in registry order.
    static constexpr std::int32_t kStructureIndex = 19;

    /// `seed` is the world seed. The ring list is the same for every chunk, so it is held here
    /// rather than recomputed from the one `generateInto` is handed: building it is 128 biome
    /// searches of 57 by 57 cells each, and doing that once per generating thread cost minutes of
    /// startup.
    StrongholdStructureGenerator(structure::ConcentricRingsPlacement rings, std::int64_t seed,
                                 NoiseBiomeAt noiseBiomeAt, std::set<std::string, std::less<>> overworldBiomes,
                                 std::int32_t seaLevel = 63);

    void generateInto(Chunk& chunk, std::int64_t seed);

    /// The stronghold nearest to a block position, or nothing in a world with none. parity:
    /// `ChunkGenerator.getNearestGeneratedStructure` walks the whole ring list keeping the smallest
    /// squared distance from each ring chunk's centre at y 32, but reports that chunk's minimum
    /// corner.
    [[nodiscard]] std::optional<std::pair<std::int32_t, std::int32_t>> locate(std::int32_t fromX,
                                                                              std::int32_t fromZ) const;

    /// parity: `terrain_adaptation: bury`, which hollows the rock the whole maze sits in.
    [[nodiscard]] densityfunction::Function beardified(densityfunction::Function base, std::int32_t chunkX,
                                                       std::int32_t chunkZ, std::int64_t seed);

private:
    /// The ring chunks whose maze can still reach this chunk.
    [[nodiscard]] std::vector<std::pair<std::int32_t, std::int32_t>> nearbyStarts(std::int32_t chunkX,
                                                                                  std::int32_t chunkZ) const;

    /// parity: `Structure.isValidBiome`, sampled at the chunk's own corner and at y 0. The rings
    /// already looked for a biome the stronghold likes; this only refuses one that is not Overworld
    /// at all.
    [[nodiscard]] bool validBiome(std::int32_t chunkX, std::int32_t chunkZ) const;

    [[nodiscard]] const std::vector<structure::StrongholdPiece>& layoutFor(std::int32_t startX, std::int32_t startZ,
                                                                           std::int64_t seed);

    void ensureRings() const;

    structure::ConcentricRingsPlacement rings_;
    std::int64_t seed_;
    NoiseBiomeAt noiseBiomeAt_;
    std::set<std::string, std::less<>> overworldBiomes_;
    std::int32_t seaLevel_;
    mutable std::mutex mutex_;
    /// parity: the order matters for `locate`, which keeps the first of two equally near rings.
    mutable std::vector<std::pair<std::int32_t, std::int32_t>> ringPositions_;
    mutable std::set<std::int64_t> ringChunks_;
    mutable bool ringsBuilt_ = false;
    std::map<std::int64_t, std::vector<structure::StrongholdPiece>> layouts_;
};

} // namespace basalt::server::world::terrain
