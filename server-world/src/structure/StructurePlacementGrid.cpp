#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/world/structure/StructurePlacementGrid.hpp>
#include <basalt/wire/common/JavaCast.hpp>

#include <cstdint>
#include <stdexcept>

namespace basalt::server::world::structure {
namespace {

constexpr std::int32_t kLegacyType2Salt = 10387320;

[[nodiscard]] std::int32_t floorDiv(std::int32_t value, std::int32_t divisor) {
    const std::int32_t quotient = value / divisor;
    return (value % divisor != 0 && ((value < 0) != (divisor < 0))) ? quotient - 1 : quotient;
}

[[nodiscard]] std::int32_t evaluate(SpreadType spreadType, random::RandomSource& random, std::int32_t limit) {
    if (spreadType == SpreadType::Triangular) {
        return (random.nextInt(limit) + random.nextInt(limit)) / 2;
    }
    return random.nextInt(limit);
}

[[nodiscard]] bool shouldGenerate(FrequencyReduction reduction, std::int64_t seed, std::int32_t salt,
                                  std::int32_t chunkX, std::int32_t chunkZ, float frequency) {
    if (frequency >= 1.0F) {
        return true;
    }
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::legacy(0));
    switch (reduction) {
    case FrequencyReduction::LegacyType1: {
        // The pillager outpost roll, which reads the chunk coordinates as block ones.
        random.setSeed(static_cast<std::int64_t>(chunkX >> 4) ^ (static_cast<std::int64_t>(chunkZ >> 4) << 4) ^ seed);
        static_cast<void>(random.nextInt());
        return random.nextInt(wire::common::toIntLikeJava(static_cast<double>(1.0F / frequency))) == 0;
    }
    case FrequencyReduction::LegacyType2:
        random::setLargeFeatureWithSalt(random, seed, chunkX, chunkZ, kLegacyType2Salt);
        return random.nextFloat() < frequency;
    case FrequencyReduction::LegacyType3:
        random::setLargeFeatureSeed(random, seed, chunkX, chunkZ);
        return random.nextDouble() < static_cast<double>(frequency);
    case FrequencyReduction::Default:
    default:
        random::setLargeFeatureWithSalt(random, seed, chunkX, chunkZ, salt);
        return random.nextFloat() < frequency;
    }
}

} // namespace

bool ExclusionZone::forbids(const std::int64_t seed, const std::int32_t chunkX, const std::int32_t chunkZ) const {
    const StructurePlacementGrid& grid = other_();
    for (std::int32_t testX = chunkX - chunkCount_; testX <= chunkX + chunkCount_; ++testX) {
        for (std::int32_t testZ = chunkZ - chunkCount_; testZ <= chunkZ + chunkCount_; ++testZ) {
            if (grid.isStructureChunk(seed, testX, testZ)) {
                return true;
            }
        }
    }
    return false;
}

StructurePlacementGrid::StructurePlacementGrid(const std::int32_t spacing, const std::int32_t separation,
                                               const std::int32_t salt, const SpreadType spreadType,
                                               const float frequency, const FrequencyReduction frequencyReduction,
                                               std::shared_ptr<const ExclusionZone> exclusionZone)
    : spacing_(spacing), separation_(separation), salt_(salt), spreadType_(spreadType), frequency_(frequency),
      frequencyReduction_(frequencyReduction), exclusionZone_(std::move(exclusionZone)) {
    if (spacing <= separation) {
        throw std::invalid_argument{"spacing must be larger than separation"};
    }
}

std::pair<std::int32_t, std::int32_t> StructurePlacementGrid::candidateChunk(const std::int64_t seed,
                                                                             const std::int32_t sourceX,
                                                                             const std::int32_t sourceZ) const {
    const std::int32_t cellX = floorDiv(sourceX, spacing_);
    const std::int32_t cellZ = floorDiv(sourceZ, spacing_);
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::legacy(0));
    random::setLargeFeatureWithSalt(random, seed, cellX, cellZ, salt_);
    const std::int32_t limit = spacing_ - separation_;
    const std::int32_t spreadX = evaluate(spreadType_, random, limit);
    const std::int32_t spreadZ = evaluate(spreadType_, random, limit);
    return {cellX * spacing_ + spreadX, cellZ * spacing_ + spreadZ};
}

bool StructurePlacementGrid::isPlacementChunk(const std::int64_t seed, const std::int32_t chunkX,
                                              const std::int32_t chunkZ) const {
    const auto [candidateX, candidateZ] = candidateChunk(seed, chunkX, chunkZ);
    return candidateX == chunkX && candidateZ == chunkZ;
}

bool StructurePlacementGrid::isStructureChunk(const std::int64_t seed, const std::int32_t chunkX,
                                              const std::int32_t chunkZ) const {
    if (!isPlacementChunk(seed, chunkX, chunkZ)) {
        return false;
    }
    if (!shouldGenerate(frequencyReduction_, seed, salt_, chunkX, chunkZ, frequency_)) {
        return false;
    }
    return exclusionZone_ == nullptr || !exclusionZone_->forbids(seed, chunkX, chunkZ);
}

} // namespace basalt::server::world::structure
