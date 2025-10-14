#pragma once

#include <basalt/server/random/RandomSource.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <utility>

namespace basalt::server::world::structure {

/// parity: `StructurePlacement.FrequencyReductionMethod`. Four ways of rolling the same probability,
/// kept apart because each seeds its random differently and vanilla still names every one of them
/// in a datapack.
enum class FrequencyReduction : std::uint8_t { Default, LegacyType1, LegacyType2, LegacyType3 };

enum class SpreadType : std::uint8_t { Linear, Triangular };

class StructurePlacementGrid;

/// parity: `StructurePlacement.ExclusionZone`, tested as a square and not a circle. The other grid
/// arrives through a lambda so the two can be built in either order.
class ExclusionZone {
public:
    ExclusionZone(std::int32_t chunkCount, std::function<const StructurePlacementGrid&()> other)
        : chunkCount_(chunkCount), other_(std::move(other)) {}

    [[nodiscard]] bool forbids(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ) const;

private:
    std::int32_t chunkCount_;
    std::function<const StructurePlacementGrid&()> other_;
};

/// parity: `RandomSpreadStructurePlacement`. One candidate chunk per `spacing`-wide cell, drawn
/// inside a `separation`-margined square so neighbouring candidates stay that far apart.
///
/// The per-cell random is a fresh legacy worldgen source, not the xoroshiro one decoration uses,
/// reseeded with `setLargeFeatureWithSalt`.
class StructurePlacementGrid {
public:
    /// Refuses a spacing that does not exceed the separation: that is a table bug, and the draw
    /// below would have an empty range.
    StructurePlacementGrid(std::int32_t spacing, std::int32_t separation, std::int32_t salt,
                           SpreadType spreadType = SpreadType::Linear, float frequency = 1.0F,
                           FrequencyReduction frequencyReduction = FrequencyReduction::Default,
                           std::shared_ptr<const ExclusionZone> exclusionZone = nullptr);

    [[nodiscard]] std::int32_t spacing() const noexcept { return spacing_; }

    [[nodiscard]] std::int32_t separation() const noexcept { return separation_; }

    [[nodiscard]] std::int32_t salt() const noexcept { return salt_; }

    /// The one candidate chunk for the grid cell containing the given chunk coordinates.
    [[nodiscard]] std::pair<std::int32_t, std::int32_t> candidateChunk(std::int64_t seed, std::int32_t sourceX,
                                                                       std::int32_t sourceZ) const;

    /// Whether the chunk is itself the candidate chunk for its own grid cell.
    [[nodiscard]] bool isPlacementChunk(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ) const;

    /// Whether the chunk really gets the structure: its own cell's candidate, then the frequency
    /// roll, then the exclusion zone. parity: `StructurePlacement.isStructureChunk`, in that order,
    /// which matters because the later tests are the expensive ones.
    [[nodiscard]] bool isStructureChunk(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ) const;

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
