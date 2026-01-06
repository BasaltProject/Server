#include "FrozenOceanShared.hpp"

#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/world/terrain/decoration/FrozenOceanDecoration.hpp>

#include <algorithm>

namespace basalt::server::world::terrain::decoration {
namespace {

using frozenocean::ChunkFeatureRegion;
using frozenocean::kSeaLevel;

constexpr std::int32_t kLocalModificationsStep = 2;
constexpr std::int32_t kSurfaceStructuresStep = 4;
constexpr std::int32_t kPackedIcebergIndex = 0;
constexpr std::int32_t kBlueIcebergIndex = 1;
constexpr std::int32_t kBlueIceIndex = 3;

[[nodiscard]] std::string_view biomePath(std::string_view id) {
    const std::size_t colon = id.find(':');
    return colon == std::string_view::npos ? id : id.substr(colon + 1);
}

[[nodiscard]] bool isFrozenOcean(const surface::SurfaceRuleEnvironment& environment, const DecorationPos& position) {
    const std::string_view biome = biomePath(environment.biomeAt(position.x, position.y, position.z));
    return biome == "frozen_ocean" || biome == "deep_frozen_ocean";
}

void placeIcebergWithRarity(ChunkFeatureRegion& region, random::RandomSource& random,
                            const surface::SurfaceRuleEnvironment& environment, std::int32_t chance,
                            std::int32_t stateId) {
    if (random.nextFloat() >= 1.0F / static_cast<float>(chance)) {
        return;
    }
    const Chunk& chunk = region.chunk();
    const DecorationPos origin{
        .x = chunk.chunkX() * 16 + random.nextInt(16), .y = kSeaLevel, .z = chunk.chunkZ() * 16 + random.nextInt(16)};
    const DecorationPos biomeProbe{.x = origin.x, .y = chunk.verticalBounds().minY(), .z = origin.z};
    if (!isFrozenOcean(environment, biomeProbe)) {
        return;
    }
    frozenocean::icebergFeature::place(region, random, origin, stateId);
}

} // namespace

const std::set<std::string, std::less<>>& frozenOceanDecoration::placedIds() {
    static const std::set<std::string, std::less<>> ids = {"iceberg_packed", "iceberg_blue", "blue_ice"};
    return ids;
}

void frozenOceanDecoration::decorateLocalModifications(Chunk& chunk, const std::int64_t seed,
                                                       const surface::SurfaceRuleEnvironment& environment) {
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    ChunkFeatureRegion region{chunk};
    random::setFeatureSeed(random, decorationSeed, kPackedIcebergIndex, kLocalModificationsStep);
    placeIcebergWithRarity(region, random, environment, 16, registry::blocks::named("packed_ice")->stateId);
    random::setFeatureSeed(random, decorationSeed, kBlueIcebergIndex, kLocalModificationsStep);
    placeIcebergWithRarity(region, random, environment, 200, registry::blocks::named("blue_ice")->stateId);
    region.flush();
}

void frozenOceanDecoration::decorateSurfaceStructures(Chunk& chunk, const std::int64_t seed,
                                                      const surface::SurfaceRuleEnvironment& environment) {
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    random::setFeatureSeed(random, decorationSeed, kBlueIceIndex, kSurfaceStructuresStep);
    ChunkFeatureRegion region{chunk};
    const std::int32_t count = random.nextInt(20);
    for (std::int32_t attempt = 0; attempt < count; ++attempt) {
        const std::int32_t x = chunk.chunkX() * 16 + random.nextInt(16);
        const std::int32_t z = chunk.chunkZ() * 16 + random.nextInt(16);
        const DecorationPos origin{.x = x, .y = 30 + random.nextInt(32), .z = z};
        if (!isFrozenOcean(environment, origin)) {
            continue;
        }
        static_cast<void>(frozenocean::blueIceFeature::place(region, random, origin));
    }
    region.flush();
}

bool frozenOceanDecoration::placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin,
                                    const std::string_view placedFeature) {
    ChunkFeatureRegion region{chunk};
    if (placedFeature == "iceberg_packed") {
        frozenocean::icebergFeature::place(region, random, origin, registry::blocks::named("packed_ice")->stateId);
    } else if (placedFeature == "iceberg_blue") {
        frozenocean::icebergFeature::place(region, random, origin, registry::blocks::named("blue_ice")->stateId);
    } else if (placedFeature == "blue_ice") {
        static_cast<void>(frozenocean::blueIceFeature::place(region, random, origin));
    } else {
        return false;
    }
    region.flush();
    return true;
}

} // namespace basalt::server::world::terrain::decoration

namespace basalt::server::world::terrain::decoration::frozenocean {

bool isFrozenWater(const std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block != nullptr && block->path() == "water";
}

std::int64_t ChunkFeatureRegion::keyOf(const DecorationPos& position) noexcept {
    // Packed, not mixed: an overlay only ever holds positions a few dozen blocks apart, so twenty six
    // bits of each horizontal coordinate tell them apart and the height goes in unsigned.
    constexpr std::int64_t kHorizontalMask = 0x3FFFFFF;
    constexpr std::int64_t kHeightBias = 2048;
    return ((static_cast<std::int64_t>(position.x) & kHorizontalMask) << 38) |
           ((static_cast<std::int64_t>(position.z) & kHorizontalMask) << 12) |
           ((static_cast<std::int64_t>(position.y) + kHeightBias) & 0xFFF);
}

std::int32_t ChunkFeatureRegion::get(const DecorationPos& position) const {
    if (const auto found = byPosition_.find(keyOf(position)); found != byPosition_.end()) {
        return changes_[found->second].second;
    }
    const ChunkHeight bounds = chunk_->verticalBounds();
    if (position.y < bounds.minY() || position.y > bounds.maxY()) {
        return registry::blocks::air().stateId;
    }
    // parity: an iceberg is wider than a chunk, so it reads into its neighbours. Guessing water
    // below sea level and air above got the open sea right and everything else wrong, and `smooth`
    // counts the sides that are not iceberg, so a wrong guess carves away blocks vanilla keeps.
    return chunk_->decorationBlockStateId(position.x, position.y, position.z);
}

void ChunkFeatureRegion::set(const DecorationPos& position, const std::int32_t stateId) {
    const ChunkHeight bounds = chunk_->verticalBounds();
    if (position.y < bounds.minY() || position.y > bounds.maxY()) {
        return;
    }
    const auto [found, fresh] = byPosition_.try_emplace(keyOf(position), changes_.size());
    if (!fresh) {
        changes_[found->second].second = stateId;
        return;
    }
    changes_.emplace_back(position, stateId);
}

void ChunkFeatureRegion::flush() {
    for (const auto& [position, stateId] : changes_) {
        // Deferred rather than dropped: the part of an iceberg that falls in the next chunk is
        // still part of the iceberg.
        chunk_->setDecorationBlockStateId(position.x, position.y, position.z, stateId);
    }
}

bool blueIceFeature::place(ChunkFeatureRegion& region, random::RandomSource& random, const DecorationPos& origin) {
    const std::int32_t packedIce = registry::blocks::named("packed_ice")->stateId;
    const std::int32_t blueIce = registry::blocks::named("blue_ice")->stateId;
    const std::int32_t ice = registry::blocks::named("ice")->stateId;
    if (origin.y > 62) {
        return false;
    }
    if (!isFrozenWater(region.get(origin)) && !isFrozenWater(region.get(down(origin)))) {
        return false;
    }
    bool touchesPackedIce = false;
    for (std::size_t index = 1; index < kAllDirections.size(); ++index) {
        if (region.get(offsetBy(origin, kAllDirections[index])) == packedIce) {
            touchesPackedIce = true;
            break;
        }
    }
    if (!touchesPackedIce) {
        return false;
    }

    region.set(origin, blueIce);
    for (std::int32_t attempt = 0; attempt < 200; ++attempt) {
        const std::int32_t yOffset = random.nextInt(5) - random.nextInt(6);
        std::int32_t horizontalRange = 3;
        if (yOffset < 2) {
            horizontalRange += yOffset / 2;
        }
        if (horizontalRange < 1) {
            continue;
        }
        const DecorationPos target =
            offsetBy(origin, random.nextInt(horizontalRange) - random.nextInt(horizontalRange), yOffset,
                     random.nextInt(horizontalRange) - random.nextInt(horizontalRange));
        const std::int32_t stateId = region.get(target);
        if (!registry::blocks::isAir(stateId) && !isFrozenWater(stateId) && stateId != packedIce && stateId != ice) {
            continue;
        }
        for (const DirectionOffset& direction : kAllDirections) {
            if (region.get(offsetBy(target, direction)) == blueIce) {
                region.set(target, blueIce);
                break;
            }
        }
    }
    return true;
}

} // namespace basalt::server::world::terrain::decoration::frozenocean
