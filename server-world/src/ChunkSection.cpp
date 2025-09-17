#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/ChunkSection.hpp>

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

} // namespace

ChunkSection::ChunkSection() : blockStateIds_(kBlocks, registry::blocks::air().stateId), biomeIds_(kBiomes, 0) {}

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
    return true;
}

std::int32_t ChunkSection::biomeId(std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ) const {
    return biomeIds_.get(biomeIndex(quartX, quartY, quartZ));
}

void ChunkSection::setBiomeId(std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ, std::int32_t biomeId) {
    biomeIds_.set(biomeIndex(quartX, quartY, quartZ), biomeId);
}

void ChunkSection::setBiomeIds(std::span<const std::int32_t> ids) { biomeIds_.setAll(ids); }

std::vector<std::int32_t> ChunkSection::biomeIds() const { return biomeIds_.toVector(); }

std::size_t ChunkSection::residentBytes() const { return blockStateIds_.residentBytes() + biomeIds_.residentBytes(); }

std::vector<std::int32_t> ChunkSection::blockStateIds() const { return blockStateIds_.toVector(); }

void ChunkSection::copyBlockStateIdsInto(std::span<std::int32_t> dest) const { blockStateIds_.copyInto(dest); }

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
}

} // namespace basalt::server::world
