#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/noise/Noise.hpp>
#include <basalt/server/world/terrain/decoration/Placement.hpp>

#include <cmath>
#include <cstdint>
#include <utility>

namespace basalt::server::world::terrain::decoration {
namespace {

/// parity: `Biome.BIOME_INFO_NOISE`, a single-octave simplex off the fixed legacy seed 2345, not the
/// world seed.
const noise::SimplexNoise& biomeInfoNoise() {
    static const noise::SimplexNoise built = [] {
        random::RandomSource source = random::RandomSource::legacy(2345LL);
        return noise::SimplexNoise{source, true};
    }();
    return built;
}

[[nodiscard]] std::vector<DecorationPos> repeated(const DecorationPos& origin, std::int32_t count) {
    std::vector<DecorationPos> positions;
    if (count <= 0) {
        return positions;
    }
    positions.assign(static_cast<std::size_t>(count), origin);
    return positions;
}

} // namespace

PlacementModifier filter(PlacementPredicate predicate) {
    return [predicate = std::move(predicate)](const PlacementContext& context, random::RandomSource& random,
                                              const DecorationPos& origin) -> std::vector<DecorationPos> {
        if (predicate(context, random, origin)) {
            return {origin};
        }
        return {};
    };
}

PlacementModifier countPlacement(IntProvider count) {
    return [count = std::move(count)](const PlacementContext&, random::RandomSource& random,
                                      const DecorationPos& origin) { return repeated(origin, count.sample(random)); };
}

PlacementModifier countPlacement(std::int32_t count) { return countPlacement(IntProvider::of(count)); }

PlacementModifier inSquarePlacement() {
    return [](const PlacementContext&, random::RandomSource& random,
              const DecorationPos& origin) -> std::vector<DecorationPos> {
        const std::int32_t x = random.nextInt(16) + origin.x;
        const std::int32_t z = random.nextInt(16) + origin.z;
        return {DecorationPos{x, origin.y, z}};
    };
}

PlacementModifier randomOffsetPlacement(std::int32_t xzSpread, std::int32_t ySpread) {
    return [xzSpread, ySpread](const PlacementContext&, random::RandomSource& random,
                               const DecorationPos& origin) -> std::vector<DecorationPos> {
        const auto trapezoid = [&random](std::int32_t range) {
            return range == 0 ? 0 : random.nextInt(range + 1) - random.nextInt(range + 1);
        };
        const std::int32_t x = origin.x + trapezoid(xzSpread);
        const std::int32_t y = origin.y + trapezoid(ySpread);
        const std::int32_t z = origin.z + trapezoid(xzSpread);
        return {DecorationPos{x, y, z}};
    };
}

PlacementModifier constantOffsetPlacement(std::int32_t xzOffset, std::int32_t yOffset) {
    return [xzOffset, yOffset](const PlacementContext&, random::RandomSource&,
                               const DecorationPos& origin) -> std::vector<DecorationPos> {
        return {DecorationPos{origin.x + xzOffset, origin.y + yOffset, origin.z + xzOffset}};
    };
}

PlacementModifier environmentScanPlacement(std::int32_t directionY, std::int32_t maxSteps,
                                           std::function<bool(std::int32_t, const DecorationPos&)> target,
                                           std::function<bool(std::int32_t, const DecorationPos&)> allowed) {
    return [directionY, maxSteps, target = std::move(target),
            allowed = std::move(allowed)](const PlacementContext& context, random::RandomSource&,
                                          const DecorationPos& origin) -> std::vector<DecorationPos> {
        Chunk& chunk = context.chunk();
        const auto stateAt = [&chunk](const DecorationPos& position) {
            return chunk.decorationBlockStateId(position.x, position.y, position.z);
        };
        if (allowed && !allowed(stateAt(origin), origin)) {
            return {};
        }
        DecorationPos position = origin;
        for (std::int32_t step = 0; step < maxSteps; ++step) {
            if (target(stateAt(position), position)) {
                return {position};
            }
            position.y += directionY;
            if (position.y < chunk.verticalBounds().minY() || position.y > chunk.verticalBounds().maxY()) {
                return {};
            }
            // Vanilla breaks here and still tests the target once at the block it stopped on.
            if (allowed && !allowed(stateAt(position), position)) {
                break;
            }
        }
        if (target(stateAt(position), position)) {
            return {position};
        }
        return {};
    };
}

PlacementModifier uniformHeightRangePlacement(std::int32_t minInclusive, std::int32_t maxInclusive) {
    return [minInclusive, maxInclusive](const PlacementContext&, random::RandomSource& random,
                                        const DecorationPos& origin) -> std::vector<DecorationPos> {
        return {DecorationPos{origin.x, minInclusive + random.nextInt(maxInclusive - minInclusive + 1), origin.z}};
    };
}

PlacementModifier surfaceRelativeThresholdFilter(HeightmapType heightmap, std::int32_t minInclusive,
                                                 std::int32_t maxInclusive) {
    return filter([heightmap, minInclusive, maxInclusive](const PlacementContext& context, random::RandomSource&,
                                                          const DecorationPos& origin) {
        // parity: the three features that use this filter, glow_lichen, lake_lava_underground and
        // underwater_magma, all name `OCEAN_FLOOR_WG`, which stops moving after the carvers.
        const auto surfaceY =
            static_cast<std::int64_t>(context.worldgenHeight(origin.x & 15, origin.z & 15, heightmap));
        const auto y = static_cast<std::int64_t>(origin.y);
        return y >= surfaceY + minInclusive && y <= surfaceY + maxInclusive;
    });
}

PlacementModifier surfaceWaterDepthFilter(std::int32_t maxWaterDepth) {
    return filter([maxWaterDepth](const PlacementContext& context, random::RandomSource&, const DecorationPos& origin) {
        const std::int32_t x = origin.x & 15;
        const std::int32_t z = origin.z & 15;
        const std::int32_t oceanFloorY = context.height(x, z, HeightmapType::OceanFloor);
        const std::int32_t worldSurfaceY = context.height(x, z, HeightmapType::WorldSurface);
        return worldSurfaceY - oceanFloorY <= maxWaterDepth;
    });
}

PlacementModifier worldgenHeightmapPlacement(HeightmapType type) {
    return [type](const PlacementContext& context, random::RandomSource&,
                  const DecorationPos& origin) -> std::vector<DecorationPos> {
        return {DecorationPos{origin.x, context.worldgenHeight(origin.x & 15, origin.z & 15, type), origin.z}};
    };
}

PlacementModifier heightmapPlacement(HeightmapType type) {
    return [type](const PlacementContext& context, random::RandomSource&,
                  const DecorationPos& origin) -> std::vector<DecorationPos> {
        const std::int32_t height = context.height(origin.x & 15, origin.z & 15, type);
        return {DecorationPos{origin.x, height, origin.z}};
    };
}

PlacementModifier noiseBasedCountPlacement(std::int32_t noiseToCountRatio, double noiseFactor, double noiseOffset) {
    return [noiseToCountRatio, noiseFactor, noiseOffset](const PlacementContext&, random::RandomSource&,
                                                         const DecorationPos& origin) {
        const auto value = static_cast<double>(biomeInfoNoise().get(origin.x / noiseFactor, origin.z / noiseFactor));
        const auto count = static_cast<std::int32_t>(std::ceil((value + noiseOffset) * noiseToCountRatio));
        return repeated(origin, count);
    };
}

PlacementModifier noiseThresholdCountPlacement(double noiseLevel, std::int32_t belowNoise, std::int32_t aboveNoise) {
    return [noiseLevel, belowNoise, aboveNoise](const PlacementContext&, random::RandomSource&,
                                                const DecorationPos& origin) {
        const auto value = static_cast<double>(biomeInfoNoise().get(origin.x / 200.0, origin.z / 200.0));
        return repeated(origin, value < noiseLevel ? belowNoise : aboveNoise);
    };
}

PlacementModifier rarityFilter(std::int32_t chance) {
    return filter([chance](const PlacementContext&, random::RandomSource& random, const DecorationPos&) {
        return random.nextFloat() < 1.0F / static_cast<float>(chance);
    });
}

PlacementModifier airTargetFilter(bool alsoCheckAbove) {
    return filter([alsoCheckAbove](const PlacementContext& context, random::RandomSource&,
                                   const DecorationPos& origin) {
        const ChunkHeight bounds = context.chunk().verticalBounds();
        if (origin.y < bounds.minY() || origin.y > bounds.maxY()) {
            return false;
        }
        if (!registry::blocks::isAir(context.chunk().decorationBlockStateId(origin.x, origin.y, origin.z))) {
            return false;
        }
        if (alsoCheckAbove) {
            if (origin.y + 1 > bounds.maxY()) {
                return false;
            }
            if (!registry::blocks::isAir(context.chunk().decorationBlockStateId(origin.x, origin.y + 1, origin.z))) {
                return false;
            }
        }
        return true;
    });
}

PlacementModifier countExtraPlacement(std::int32_t count, float chance, std::int32_t extra) {
    return [count, chance, extra](const PlacementContext&, random::RandomSource& random, const DecorationPos& origin) {
        const auto weight = static_cast<std::int32_t>(1.0F / chance);
        const std::int32_t n = random.nextInt(weight) == weight - 1 ? count + extra : count;
        return repeated(origin, n);
    };
}

void setExternalDecorationBlockStateId(
    Chunk& chunk, std::vector<std::pair<DecorationPos, std::int32_t>>& externalWrites,
    const std::function<std::optional<std::int32_t>(std::int32_t, std::int32_t, std::int32_t)>& externalBlockState,
    const DecorationPos& position, std::int32_t stateId) {
    std::int32_t expectedStateId = registry::blocks::air().stateId;
    bool known = false;
    for (auto& [written, value] : externalWrites) {
        if (written == position) {
            expectedStateId = value;
            value = stateId;
            known = true;
            break;
        }
    }
    if (!known) {
        if (externalBlockState) {
            if (const std::optional<std::int32_t> observed = externalBlockState(position.x, position.y, position.z);
                observed.has_value()) {
                expectedStateId = *observed;
            }
        }
        externalWrites.emplace_back(position, stateId);
    }
    chunk.setDecorationBlockStateId(position.x, position.y, position.z, stateId, expectedStateId);
}

namespace {

void placeThrough(const PlacementContext& context, random::RandomSource& random, const DecorationPos& position,
                  const std::vector<PlacementModifier>& modifiers, std::size_t index,
                  const std::function<void(const DecorationPos&)>& place) {
    if (index == modifiers.size()) {
        place(position);
        return;
    }
    for (const DecorationPos& next : modifiers[index](context, random, position)) {
        placeThrough(context, random, next, modifiers, index + 1, place);
    }
}

} // namespace

void placeFrom(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin,
               const std::vector<PlacementModifier>& modifiers,
               const std::function<void(const DecorationPos&)>& place) {
    const PlacementContext context{chunk};
    placeThrough(context, random, origin, modifiers, 0, place);
}

} // namespace basalt::server::world::terrain::decoration
