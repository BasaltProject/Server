#pragma once

#include <basalt/server/world/PalettedContainer.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace basalt::server::world {

/// One 16x16x16 slice of a chunk. Block state ids default to air's.
class ChunkSection {
public:
    static constexpr std::size_t kBlocks = 16 * 16 * 16;
    /// Biomes are kept at quart resolution, 4x4x4 to a section.
    static constexpr std::size_t kBiomes = 4 * 4 * 4;

    ChunkSection();

    [[nodiscard]] std::int32_t blockStateId(std::int32_t x, std::int32_t y, std::int32_t z) const;

    /// Whether the block actually changed, so callers can track a chunk as modified without
    /// re-reading it.
    bool setBlockStateId(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId);

    [[nodiscard]] std::int32_t biomeId(std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ) const;

    void setBiomeId(std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ, std::int32_t biomeId);

    /// Overwrites all 64 quart-resolution biome ids in wire order.
    void setBiomeIds(std::span<const std::int32_t> ids);

    /// The raw biome ids, in `(y * 4 + z) * 4 + x` order: the layout the biome container expects.
    [[nodiscard]] std::vector<std::int32_t> biomeIds() const;

    /// Blocks that count toward lighting, which is to say not air. parity: the per-section block
    /// count vanilla writes.
    [[nodiscard]] std::int32_t nonAirBlockCount() const noexcept { return nonAirBlockCount_; }

    /// Blocks whose state carries water, matching vanilla's section fluid-state count.
    [[nodiscard]] std::int32_t nonEmptyFluidCount() const noexcept { return nonEmptyFluidCount_; }

    /// Approximate bytes both palettes keep resident, for the timings report's memory breakdown.
    [[nodiscard]] std::size_t residentBytes() const;

    /// Whether this section's palette holds any state satisfying `predicate`.
    template <typename Predicate>
    [[nodiscard]] bool mayContain(Predicate&& predicate) const {
        return blockStateIds_.anyValue(std::forward<Predicate>(predicate));
    }

    /// The raw state ids, in `(y * 16 + z) * 16 + x` order.
    [[nodiscard]] std::vector<std::int32_t> blockStateIds() const;

    /// The same values into a caller-owned buffer, so a save reuses one across a chunk's sections.
    void copyBlockStateIdsInto(std::span<std::int32_t> dest) const;

    void copyBiomeIdsInto(std::span<std::int32_t> dest) const;

    /// Overwrites every state id at once, in the same order `blockStateIds` gives them back.
    void setBlockStateIds(std::span<const std::int32_t> ids);

private:
    // Both counts are kept on write rather than recounted on read: recounting scanned all 4096
    // entries every time, which is once per section per chunk send.
    PalettedContainer blockStateIds_;
    PalettedContainer biomeIds_;
    std::int32_t nonAirBlockCount_ = 0;
    std::int32_t nonEmptyFluidCount_ = 0;
};

} // namespace basalt::server::world
