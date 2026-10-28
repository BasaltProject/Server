#pragma once

#include <basalt/server/random/RandomSource.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

namespace basalt::server::world::structure {

/// parity: `StructurePlacement.FrequencyReductionMethod`.
enum class FrequencyReduction : std::uint8_t { Default, LegacyType1, LegacyType2, LegacyType3 };

enum class SpreadType : std::uint8_t { Linear, Triangular };

class StructurePlacementGrid;

/// parity: `StructurePlacement.ExclusionZone`.
class ExclusionZone {
public:
    ExclusionZone(std::int32_t chunkCount, std::function<const StructurePlacementGrid&()> other)
        : chunkCount_(chunkCount), other_(std::move(other)) {}

    [[nodiscard]] bool forbids(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ) const;

private:
    std::int32_t chunkCount_;
    std::function<const StructurePlacementGrid&()> other_;
};

/// parity: `RandomSpreadStructurePlacement`.
class StructurePlacementGrid {
public:
    StructurePlacementGrid(std::int32_t spacing, std::int32_t separation, std::int32_t salt,
                           SpreadType spreadType = SpreadType::Linear, float frequency = 1.0F,
                           FrequencyReduction frequencyReduction = FrequencyReduction::Default,
                           std::shared_ptr<const ExclusionZone> exclusionZone = nullptr);

    [[nodiscard]] std::int32_t spacing() const noexcept { return spacing_; }

    [[nodiscard]] std::int32_t separation() const noexcept { return separation_; }

    [[nodiscard]] std::int32_t salt() const noexcept { return salt_; }

    [[nodiscard]] std::pair<std::int32_t, std::int32_t> candidateChunk(std::int64_t seed, std::int32_t sourceX,
                                                                       std::int32_t sourceZ) const;

    [[nodiscard]] bool isPlacementChunk(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ) const;

    /// The frequency roll, then the exclusion zone. parity:
    [[nodiscard]] bool isStructureChunk(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ) const;

    /// parity: `ChunkGenerator.getNearestGeneratedStructure` for a random spread set.
    [[nodiscard]] std::optional<std::pair<std::int32_t, std::int32_t>>
    locate(std::int64_t seed, std::int32_t fromChunkX, std::int32_t fromChunkZ, std::int32_t maxCells,
           const std::function<bool(std::int32_t, std::int32_t)>& startsAt) const;

private:
    std::int32_t spacing_;
    std::int32_t separation_;
    std::int32_t salt_;
    SpreadType spreadType_;
    float frequency_;
    FrequencyReduction frequencyReduction_;
    std::shared_ptr<const ExclusionZone> exclusionZone_;
};

} // namespace basalt::server::world::structure
