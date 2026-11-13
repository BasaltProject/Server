#pragma once

#include <basalt/server/world/PalettedContainer.hpp>

#include <atomic>
#include <cstdint>
#include <span>
#include <vector>

namespace basalt::server::world {

/// Names one exact content of a section: every write clears it, and a copy shares it until either is written.
class ContentVersion {
public:
    ContentVersion() = default;

    ContentVersion(const ContentVersion& other) noexcept : value_(other.value_.load(std::memory_order_relaxed)) {}

    ContentVersion& operator=(const ContentVersion& other) noexcept {
        value_.store(other.value_.load(std::memory_order_relaxed), std::memory_order_relaxed);
        return *this;
    }

    void clear() noexcept { value_.store(0, std::memory_order_relaxed); }

    [[nodiscard]] std::uint64_t get() const noexcept;

private:
    mutable std::atomic<std::uint64_t> value_{0};
};

class ChunkSection {
public:
    static constexpr std::size_t kBlocks = 16 * 16 * 16;
    static constexpr std::size_t kBiomes = 4 * 4 * 4;

    ChunkSection();

    [[nodiscard]] std::int32_t blockStateId(std::int32_t x, std::int32_t y, std::int32_t z) const;

    bool setBlockStateId(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId);

    [[nodiscard]] std::int32_t biomeId(std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ) const;

    void setBiomeId(std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ, std::int32_t biomeId);

    void setBiomeIds(std::span<const std::int32_t> ids);

    [[nodiscard]] std::vector<std::int32_t> biomeIds() const;

    /// Blocks that count toward lighting, which is to say not air. parity: the per-section block
    [[nodiscard]] std::int32_t nonAirBlockCount() const noexcept { return nonAirBlockCount_; }

    [[nodiscard]] std::int32_t nonEmptyFluidCount() const noexcept { return nonEmptyFluidCount_; }

    [[nodiscard]] std::size_t residentBytes() const;

    template <typename Predicate>
    [[nodiscard]] bool mayContain(Predicate&& predicate) const {
        return blockStateIds_.anyValue(std::forward<Predicate>(predicate));
    }

    template <typename Predicate>
    [[nodiscard]] bool mayContainBiome(Predicate&& predicate) const {
        return biomeIds_.anyValue(std::forward<Predicate>(predicate));
    }

    /// Called when the chunk goes where other threads can reach it.
    void share() noexcept {
        blockStateIds_.share();
        biomeIds_.share();
    }

    [[nodiscard]] std::vector<std::int32_t> blockStateIds() const;

    void copyBlockStateIdsInto(std::span<std::int32_t> dest) const;

    void copyMappedBlockStatesInto(std::span<std::uint8_t> dest, std::span<const std::int32_t> table,
                                   std::uint8_t fallback) const;

    void copyBiomeIdsInto(std::span<std::int32_t> dest) const;

    void setBlockStateIds(std::span<const std::int32_t> ids);

    /// Never zero; equal values mean equal blocks, biomes and counts.
    [[nodiscard]] std::uint64_t contentVersion() const noexcept { return version_.get(); }

private:
    ContentVersion version_;
    PalettedContainer blockStateIds_;
    PalettedContainer biomeIds_;
    std::int32_t nonAirBlockCount_ = 0;
    std::int32_t nonEmptyFluidCount_ = 0;
};

} // namespace basalt::server::world
