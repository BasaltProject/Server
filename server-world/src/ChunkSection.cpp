#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/ChunkSection.hpp>

#include <cstdint>
#include <stdexcept>

namespace basalt::server::world {
namespace {

[[nodiscard]] std::size_t index(std::int32_t x, std::int32_t y, std::int32_t z) {
    if (x < 0 || x > 15 || y < 0 || y > 15 || z < 0 || z > 15) {
        throw std::out_of_range{"coordinates out of section bounds"};
    }
    return static_cast<std::size_t>((y << 8) | (z << 4) | x);
}

[[nodiscard]] std::size_t biomeIndex(std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ) {
    if (quartX < 0 || quartX > 3 || quartY < 0 || quartY > 3 || quartZ < 0 || quartZ > 3) {
        throw std::out_of_range{"quart coordinates out of section bounds"};
    }
    return static_cast<std::size_t>((quartY << 4) | (quartZ << 2) | quartX);
}

std::atomic<std::uint64_t> gNextVersion{1};

} // namespace

std::uint64_t ContentVersion::get() const noexcept {
    std::uint64_t current = value_.load(std::memory_order_relaxed);
    if (current == 0) {
        const std::uint64_t fresh = gNextVersion.fetch_add(1, std::memory_order_relaxed);
        if (value_.compare_exchange_strong(current, fresh, std::memory_order_relaxed)) {
            return fresh;
        }
    }
    return current;
}

ChunkSection::ChunkSection() : blockStateIds_(kBlocks, registry::blocks::air().stateId), biomeIds_(kBiomes, 0) {}

bool ChunkSection::mayContainRandomTicking(bool (*ticksRandomly)(std::int32_t)) const {
    const std::uint64_t version = version_.get();
    const std::uint64_t memo = randomTickingMemo_.load(std::memory_order_relaxed);
    if (memo != 0 && (memo >> 1) == version) {
        return (memo & 1U) != 0;
    }
    const bool found = blockStateIds_.anyValue(ticksRandomly);
    randomTickingMemo_.store((version << 1) | (found ? 1U : 0U), std::memory_order_relaxed);
    return found;
}

void ChunkSection::prefetchBlock(std::int32_t x, std::int32_t y, std::int32_t z) const {
    blockStateIds_.prefetch(index(x, y, z));
}

std::int32_t ChunkSection::blockStateId(std::int32_t x, std::int32_t y, std::int32_t z) const {
    return blockStateIds_.get(index(x, y, z));
}

bool ChunkSection::setBlockStateId(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) {
    const std::size_t at = index(x, y, z);
    const std::int32_t previous = blockStateIds_.get(at);
    if (!blockStateIds_.set(at, stateId)) {
        return false;
    }
    if (!registry::blocks::isAir(previous)) {
        nonAirBlockCount_--;
    }
    if (!registry::blocks::isAir(stateId)) {
        nonAirBlockCount_++;
    }
    if (registry::blockFluids::hasFluid(previous)) {
        nonEmptyFluidCount_--;
    }
    if (registry::blockFluids::hasFluid(stateId)) {
        nonEmptyFluidCount_++;
    }
    version_.clear();
    return true;
}

std::int32_t ChunkSection::biomeId(std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ) const {
    return biomeIds_.get(biomeIndex(quartX, quartY, quartZ));
}

void ChunkSection::setBiomeId(std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ, std::int32_t biomeId) {
    biomeIds_.set(biomeIndex(quartX, quartY, quartZ), biomeId);
    version_.clear();
}

void ChunkSection::setBiomeIds(std::span<const std::int32_t> ids) {
    biomeIds_.setAll(ids);
    version_.clear();
}

std::vector<std::int32_t> ChunkSection::biomeIds() const { return biomeIds_.toVector(); }

std::size_t ChunkSection::residentBytes() const { return blockStateIds_.residentBytes() + biomeIds_.residentBytes(); }

std::vector<std::int32_t> ChunkSection::blockStateIds() const { return blockStateIds_.toVector(); }

void ChunkSection::copyBlockStateIdsInto(std::span<std::int32_t> dest) const { blockStateIds_.copyInto(dest); }

void ChunkSection::copyMappedBlockStatesInto(std::span<std::uint8_t> dest, std::span<const std::int32_t> table,
                                             std::uint8_t fallback) const {
    blockStateIds_.copyMappedInto(dest, table, fallback);
}

void ChunkSection::copyBiomeIdsInto(std::span<std::int32_t> dest) const { biomeIds_.copyInto(dest); }

void ChunkSection::setBlockStateIds(std::span<const std::int32_t> ids) {
    blockStateIds_.setAll(ids);
    nonAirBlockCount_ = 0;
    nonEmptyFluidCount_ = 0;
    for (const std::int32_t stateId : ids) {
        if (!registry::blocks::isAir(stateId)) {
            nonAirBlockCount_++;
        }
        if (registry::blockFluids::hasFluid(stateId)) {
            nonEmptyFluidCount_++;
        }
    }
    version_.clear();
}

bool ChunkSection::restorePacked(const PackedContainer& blocks, const PackedContainer& biomes) {
    thread_local std::vector<std::uint32_t> counts;
    if (!blockStateIds_.assignPacked(blocks.palette, blocks.bits, blocks.wordBytes, counts)) {
        return false;
    }
    nonAirBlockCount_ = 0;
    nonEmptyFluidCount_ = 0;
    for (std::size_t i = 0; i < blocks.palette.size(); ++i) {
        if (!registry::blocks::isAir(blocks.palette[i])) {
            nonAirBlockCount_ += static_cast<std::int32_t>(counts[i]);
        }
        if (registry::blockFluids::hasFluid(blocks.palette[i])) {
            nonEmptyFluidCount_ += static_cast<std::int32_t>(counts[i]);
        }
    }
    const bool restored = biomeIds_.assignPacked(biomes.palette, biomes.bits, biomes.wordBytes, counts);
    version_.clear();
    return restored;
}

} // namespace basalt::server::world
