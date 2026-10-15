#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/densityfunction/DensityFunction.hpp>
#include <basalt/server/world/structure/ConcentricRingsPlacement.hpp>
#include <basalt/server/world/structure/StrongholdLayout.hpp>
#include <basalt/server/world/terrain/StructureBeardifier.hpp>

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
class StrongholdStructureGenerator {
public:
    using NoiseBiomeAt = std::function<std::string(std::int32_t, std::int32_t, std::int32_t)>;

    static constexpr std::string_view kName = "stronghold";
    static constexpr std::int32_t kStructureIndex = 37;

    StrongholdStructureGenerator(structure::ConcentricRingsPlacement rings, std::int64_t seed,
                                 NoiseBiomeAt noiseBiomeAt, std::set<std::string, std::less<>> overworldBiomes,
                                 std::int32_t seaLevel = 63);

    void generateInto(Chunk& chunk, std::int64_t seed);

    /// parity: `PlaceCommand.placeStructure`: `StrongholdStructure.findGenerationPoint` never.
    [[nodiscard]] std::optional<structure::StructureBoundingBox> placeBoundsAt(std::int64_t seed, std::int32_t chunkX,
                                                                               std::int32_t chunkZ);

    void placeAt(Chunk& chunk, std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ);

    [[nodiscard]] std::optional<std::pair<std::int32_t, std::int32_t>> locate(std::int32_t fromX,
                                                                              std::int32_t fromZ) const;

    /// parity: `terrain_adaptation: bury`, which hollows the rock the whole maze sits in.
    void addBeard(StructureBeard& beard, std::int32_t chunkX, std::int32_t chunkZ, std::int64_t seed);

private:
    [[nodiscard]] std::vector<std::pair<std::int32_t, std::int32_t>> nearbyStarts(std::int32_t chunkX,
                                                                                  std::int32_t chunkZ) const;

    /// parity: `Structure.isValidBiome`.
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
    /// parity: the order matters for `locate`.
    mutable std::vector<std::pair<std::int32_t, std::int32_t>> ringPositions_;
    mutable std::set<std::int64_t> ringChunks_;
    mutable bool ringsBuilt_ = false;
    std::map<std::int64_t, std::vector<structure::StrongholdPiece>> layouts_;
};

} // namespace basalt::server::world::terrain
