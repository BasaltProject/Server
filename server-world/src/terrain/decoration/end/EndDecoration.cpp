#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockStateFamilies.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/DensityChunkSampler.hpp>
#include <basalt/server/world/terrain/decoration/end/EndDecoration.hpp>
#include <basalt/server/world/terrain/decoration/end/EndFeatureBlocks.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

namespace basalt::server::world::terrain::decoration::end {
namespace {

constexpr std::int32_t kRawGenerationStep = 0;
constexpr std::int32_t kVegetalDecorationStep = 9;
constexpr std::int32_t kEndIslandIndex = 0;
constexpr std::int32_t kChorusPlantIndex = 0;
constexpr std::int32_t kEndIslandRarity = 14;
constexpr std::int32_t kEndIslandMinY = 55;
constexpr std::int32_t kEndIslandMaxY = 70;
constexpr std::int32_t kEndGenerationDepth = 128;
constexpr std::int32_t kMaxBranchDepth = 4;
constexpr std::int32_t kMaxHorizontalSpread = 8;

// parity: the fixed `end_platform` position and radius.
constexpr std::int32_t kPlatformOriginX = 100;
constexpr std::int32_t kPlatformOriginY = 49;
constexpr std::int32_t kPlatformOriginZ = 0;
constexpr std::int32_t kPlatformRadius = 2;

struct EndPos {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;

    friend auto operator<=>(const EndPos&, const EndPos&) = default;
};

struct HorizontalDirection {
    std::int32_t dx;
    std::int32_t dz;
    std::size_t opposite;
};

constexpr std::array<HorizontalDirection, 4> kHorizontalDirections{
    HorizontalDirection{.dx = 0, .dz = -1, .opposite = 2}, HorizontalDirection{.dx = 1, .dz = 0, .opposite = 3},
    HorizontalDirection{.dx = 0, .dz = 1, .opposite = 0}, HorizontalDirection{.dx = -1, .dz = 0, .opposite = 1}};

[[nodiscard]] EndPos relative(const EndPos& position, std::size_t direction) {
    const HorizontalDirection& step = kHorizontalDirections[direction];
    return EndPos{.x = position.x + step.dx, .y = position.y, .z = position.z + step.dz};
}

[[nodiscard]] EndPos above(const EndPos& position, std::int32_t distance = 1) {
    return EndPos{.x = position.x, .y = position.y + distance, .z = position.z};
}

[[nodiscard]] EndPos below(const EndPos& position) {
    return EndPos{.x = position.x, .y = position.y - 1, .z = position.z};
}

const std::set<std::string, std::less<>>& chorusConnections() {
    static const std::set<std::string, std::less<>> paths = {"chorus_plant", "chorus_flower"};
    return paths;
}

const std::set<std::string, std::less<>>& chorusDownConnections() {
    static const std::set<std::string, std::less<>> paths = {"chorus_plant", "chorus_flower", "end_stone"};
    return paths;
}

/// The read and write view a chorus plant grows through: it reaches across the chunk edge, and what
/// is out there has to be sampled from the router rather than read.
class ChorusWorld {
public:
    ChorusWorld(Chunk& chunk, const EndNoiseRouter& router) : chunk_(&chunk), router_(&router) {}

    [[nodiscard]] bool isEmpty(const EndPos& position) { return registry::blocks::isAir(stateAt(position)); }

    [[nodiscard]] std::string pathAt(const EndPos& position) {
        const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateAt(position));
        return block == nullptr ? std::string{} : block->path();
    }

    [[nodiscard]] bool allHorizontalNeighborsEmpty(const EndPos& position, std::int32_t ignored) {
        for (std::size_t direction = 0; direction < kHorizontalDirections.size(); ++direction) {
            if (static_cast<std::int32_t>(direction) == ignored) {
                continue;
            }
            if (!isEmpty(relative(position, direction))) {
                return false;
            }
        }
        return true;
    }

    void setPlant(const EndPos& position) {
        const std::int32_t stateId = registry::blockStateFamilies::chorusPlantStateId(
            chorusDownConnections().contains(pathAt(below(position))),
            chorusConnections().contains(pathAt(relative(position, 1))),
            chorusConnections().contains(pathAt(relative(position, 0))),
            chorusConnections().contains(pathAt(relative(position, 2))),
            chorusConnections().contains(pathAt(above(position))),
            chorusConnections().contains(pathAt(relative(position, 3))));
        setBlock(position, stateId);
    }

    void setBlock(const EndPos& position, std::int32_t stateId) {
        const ChunkHeight bounds = chunk_->verticalBounds();
        if (position.y < bounds.minY() || position.y > bounds.maxY()) {
            return;
        }
        const std::int32_t expectedStateId = stateAt(position);
        outsideWrites_[position] = stateId;
        chunk_->setDecorationBlockStateId(position.x, position.y, position.z, stateId, expectedStateId);
    }

private:
    [[nodiscard]] std::int32_t stateAt(const EndPos& position) {
        const auto written = outsideWrites_.find(position);
        if (written != outsideWrites_.end()) {
            return written->second;
        }
        const ChunkHeight bounds = chunk_->verticalBounds();
        if (position.y < bounds.minY() || position.y > bounds.maxY()) {
            return registry::blocks::air().stateId;
        }
        if ((position.x >> 4) == chunk_->chunkX() && (position.z >> 4) == chunk_->chunkZ()) {
            return chunk_->blockStateId(position.x & 15, position.y, position.z & 15);
        }
        if (position.y < 0 || position.y >= kEndGenerationDepth) {
            return registry::blocks::air().stateId;
        }
        const std::int32_t sampleChunkX = position.x >> 4;
        const std::int32_t sampleChunkZ = position.z >> 4;
        const std::pair<std::int32_t, std::int32_t> key{sampleChunkX, sampleChunkZ};
        auto sampler = samplers_.find(key);
        if (sampler == samplers_.end()) {
            sampler =
                samplers_
                    .emplace(key, std::make_unique<DensityChunkSampler>(router_->finalDensity, sampleChunkX,
                                                                        sampleChunkZ, 0, kEndGenerationDepth, 8, 4))
                    .first;
        }
        const double density = sampler->second->sample(position.x & 15, position.y, position.z & 15);
        return density > 0.0 ? registry::blocks::named("end_stone")->stateId : registry::blocks::air().stateId;
    }

    Chunk* chunk_;
    const EndNoiseRouter* router_;
    std::map<EndPos, std::int32_t> outsideWrites_;
    std::map<std::pair<std::int32_t, std::int32_t>, std::unique_ptr<DensityChunkSampler>> samplers_;
};

void growTreeRecursive(ChorusWorld& world, const EndPos& current, random::RandomSource& random, const EndPos& start,
                       std::int32_t depth) {
    std::int32_t height = random.nextInt(4) + 1;
    if (depth == 0) {
        height++;
    }
    for (std::int32_t offsetY = 1; offsetY <= height; ++offsetY) {
        const EndPos target = above(current, offsetY);
        if (!world.allHorizontalNeighborsEmpty(target, -1)) {
            return;
        }
        world.setPlant(target);
        world.setPlant(below(target));
    }

    bool placedStem = false;
    if (depth < kMaxBranchDepth) {
        std::int32_t stems = random.nextInt(4);
        if (depth == 0) {
            stems++;
        }
        for (std::int32_t stem = 0; stem < stems; ++stem) {
            const auto direction =
                static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(kHorizontalDirections.size())));
            const EndPos target = relative(above(current, height), direction);
            const auto opposite = static_cast<std::int32_t>(kHorizontalDirections[direction].opposite);
            if (std::abs(target.x - start.x) < kMaxHorizontalSpread &&
                std::abs(target.z - start.z) < kMaxHorizontalSpread && world.isEmpty(target) &&
                world.isEmpty(below(target)) && world.allHorizontalNeighborsEmpty(target, opposite)) {
                placedStem = true;
                world.setPlant(target);
                world.setPlant(relative(target, static_cast<std::size_t>(opposite)));
                growTreeRecursive(world, target, random, start, depth + 1);
            }
        }
    }
    if (!placedStem) {
        world.setBlock(above(current, height), registry::blockStateFamilies::chorusFlowerStateId(5));
    }
}

void generatePlant(ChorusWorld& world, const EndPos& origin, random::RandomSource& random) {
    world.setPlant(origin);
    growTreeRecursive(world, origin, random, origin, 0);
}

void placeIsland(Chunk& chunk, random::RandomSource& random, std::int32_t originX, std::int32_t originY,
                 std::int32_t originZ) {
    const ChunkHeight bounds = chunk.verticalBounds();
    endIslandFeature::placeBlocks(random, originX, originY, originZ,
                                  [&chunk, bounds](std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t state) {
                                      if (y >= bounds.minY() && y <= bounds.maxY()) {
                                          chunk.setDecorationBlockStateId(x, y, z, state);
                                      }
                                  });
}

} // namespace

void endRawDecoration::placeAt(Chunk& chunk, random::RandomSource& random, const std::int32_t x, const std::int32_t y,
                               const std::int32_t z) {
    placeIsland(chunk, random, x, y, z);
}

void endRawDecoration::decorate(Chunk& chunk, const std::int64_t seed, const TheEndBiomeSource& biomeSource) {
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    random::setFeatureSeed(random, decorationSeed, kEndIslandIndex, kRawGenerationStep);
    if (random.nextFloat() >= 1.0F / static_cast<float>(kEndIslandRarity)) {
        return;
    }
    const std::int32_t count = random.nextInt(4) < 3 ? 1 : 2;
    for (std::int32_t island = 0; island < count; ++island) {
        const std::int32_t x = chunk.chunkX() * 16 + random.nextInt(16);
        const std::int32_t z = chunk.chunkZ() * 16 + random.nextInt(16);
        const std::int32_t y = kEndIslandMinY + random.nextInt(kEndIslandMaxY - kEndIslandMinY + 1);
        if (biomeSource.blockBiomeAt(x, y, z) == "minecraft:small_end_islands") {
            placeIsland(chunk, random, x, y, z);
        }
    }
}

void endTopLayerDecoration::decorate(Chunk& chunk, const TheEndBiomeSource& biomeSource) {
    if (biomeSource.blockBiomeAt(kPlatformOriginX, kPlatformOriginY, kPlatformOriginZ) != "minecraft:the_end") {
        return;
    }
    const std::int32_t minChunkZ = (kPlatformOriginZ - kPlatformRadius) >> 4;
    const std::int32_t maxChunkZ = (kPlatformOriginZ + kPlatformRadius) >> 4;
    if (chunk.chunkX() != (kPlatformOriginX >> 4) || chunk.chunkZ() < minChunkZ || chunk.chunkZ() > maxChunkZ) {
        return;
    }
    const ChunkHeight bounds = chunk.verticalBounds();
    const std::int32_t obsidian = registry::blocks::named("obsidian")->stateId;
    const std::int32_t air = registry::blocks::air().stateId;
    const std::int32_t fromZ = std::max(chunk.chunkZ() * 16, kPlatformOriginZ - kPlatformRadius);
    const std::int32_t toZ = std::min(chunk.chunkZ() * 16 + 15, kPlatformOriginZ + kPlatformRadius);
    for (std::int32_t z = fromZ; z <= toZ; ++z) {
        for (std::int32_t x = kPlatformOriginX - kPlatformRadius; x <= kPlatformOriginX + kPlatformRadius; ++x) {
            if (kPlatformOriginY - 1 >= bounds.minY() && kPlatformOriginY - 1 <= bounds.maxY()) {
                chunk.setBlockStateId(x & 15, kPlatformOriginY - 1, z & 15, obsidian);
            }
            for (std::int32_t y = kPlatformOriginY; y <= kPlatformOriginY + 2; ++y) {
                if (y >= bounds.minY() && y <= bounds.maxY()) {
                    chunk.setBlockStateId(x & 15, y, z & 15, air);
                }
            }
        }
    }
}

bool endVegetalDecoration::placeAt(Chunk& chunk, random::RandomSource& random, const EndNoiseRouter& router,
                                   const std::int32_t x, const std::int32_t y, const std::int32_t z) {
    ChorusWorld world{chunk, router};
    const EndPos origin{.x = x, .y = y, .z = z};
    if (!world.isEmpty(origin) || world.pathAt(below(origin)) != "end_stone") {
        return false;
    }
    generatePlant(world, origin, random);
    return true;
}

void endVegetalDecoration::decorate(Chunk& chunk, const std::int64_t seed, const EndNoiseRouter& router,
                                    const TheEndBiomeSource& biomeSource) {
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    random::setFeatureSeed(random, decorationSeed, kChorusPlantIndex, kVegetalDecorationStep);
    ChorusWorld world{chunk, router};
    const std::int32_t count = random.nextInt(5);
    for (std::int32_t attempt = 0; attempt < count; ++attempt) {
        const std::int32_t x = chunk.chunkX() * 16 + random.nextInt(16);
        const std::int32_t z = chunk.chunkZ() * 16 + random.nextInt(16);
        const std::int32_t y = chunk.heightAt(x & 15, z & 15, HeightmapType::MotionBlocking);
        if (biomeSource.blockBiomeAt(x, y, z) != "minecraft:end_highlands") {
            continue;
        }
        const EndPos origin{.x = x, .y = y, .z = z};
        if (world.isEmpty(origin) && world.pathAt(below(origin)) == "end_stone") {
            generatePlant(world, origin, random);
        }
    }
}

} // namespace basalt::server::world::terrain::decoration::end
