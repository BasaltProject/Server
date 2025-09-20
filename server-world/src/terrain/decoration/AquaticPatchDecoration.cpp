#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockStateFamilies.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/AquaticPatchDecoration.hpp>
#include <basalt/server/world/terrain/decoration/Placement.hpp>

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace basalt::server::world::terrain::decoration {
namespace {

constexpr std::int32_t kVegetalDecorationStep = 9;

[[nodiscard]] std::int32_t stateIdOf(std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    if (found == nullptr) {
        throw std::runtime_error{"unknown block '" + std::string{path} + "'"};
    }
    return found->stateId;
}

[[nodiscard]] std::string_view biomePath(std::string_view id) {
    const std::size_t colon = id.find(':');
    return colon == std::string_view::npos ? id : id.substr(colon + 1);
}

[[nodiscard]] bool isWaterBlock(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block != nullptr && block->path() == "water";
}

/// The patch region plus the ocean-floor scan the aquatic features need. Outside the chunk that scan
/// walks the neighbour reader rather than the heightmap, because a heightmap only exists for the
/// chunk that owns it.
class AquaticRegion {
public:
    AquaticRegion(Chunk& chunk, ExternalStateReader externalBlockState, BlockWriteObserver observer)
        : chunk_(&chunk), inner_(chunk, std::move(externalBlockState), std::move(observer)) {}

    [[nodiscard]] std::int32_t get(const DecorationPos& position) const { return inner_.get(position); }

    [[nodiscard]] std::int32_t oceanFloor(std::int32_t x, std::int32_t z) const {
        if ((x >> 4) == chunk_->chunkX() && (z >> 4) == chunk_->chunkZ()) {
            return chunk_->heightAt(x & 15, z & 15, HeightmapType::OceanFloor);
        }
        const ChunkHeight bounds = chunk_->verticalBounds();
        for (std::int32_t y = bounds.maxY(); y >= bounds.minY(); --y) {
            if (heightmapMatches(HeightmapType::OceanFloor, get(DecorationPos{x, y, z}))) {
                return y + 1;
            }
        }
        return bounds.minY();
    }

    void set(const DecorationPos& position, std::int32_t stateId) {
        const ChunkHeight bounds = chunk_->verticalBounds();
        if (position.y < bounds.minY() || position.y > bounds.maxY()) {
            return;
        }
        inner_.set(position, stateId);
    }

private:
    Chunk* chunk_;
    PatchRegion inner_;
};

enum class AquaticKind : std::uint8_t { Seagrass, SeaPickle, Kelp };

struct AquaticSpec {
    std::string_view name;
    std::int32_t index;
    AquaticKind kind;
    std::vector<std::string> biomes;
    std::int32_t bodyCount = 1;
    float tallProbability = 0.0F;
    std::optional<std::int32_t> rarity;
    std::optional<std::int32_t> noiseToCountRatio;
};

const std::vector<AquaticSpec>& specs() {
    static const std::vector<AquaticSpec> built = {
        {.name = "seagrass_swamp",
         .index = 87,
         .kind = AquaticKind::Seagrass,
         .biomes = {"mangrove_swamp", "swamp"},
         .bodyCount = 64,
         .tallProbability = 0.6F},
        {.name = "seagrass_river",
         .index = 90,
         .kind = AquaticKind::Seagrass,
         .biomes = {"river"},
         .bodyCount = 48,
         .tallProbability = 0.4F},
        {.name = "seagrass_warm",
         .index = 97,
         .kind = AquaticKind::Seagrass,
         .biomes = {"lukewarm_ocean", "warm_ocean"},
         .bodyCount = 80,
         .tallProbability = 0.3F},
        {.name = "sea_pickle", .index = 98, .kind = AquaticKind::SeaPickle, .biomes = {"warm_ocean"}, .rarity = 16},
        {.name = "seagrass_deep_warm",
         .index = 99,
         .kind = AquaticKind::Seagrass,
         .biomes = {"deep_lukewarm_ocean"},
         .bodyCount = 80,
         .tallProbability = 0.8F},
        {.name = "kelp_warm",
         .index = 100,
         .kind = AquaticKind::Kelp,
         .biomes = {"deep_lukewarm_ocean", "lukewarm_ocean"},
         .noiseToCountRatio = 80},
        {.name = "seagrass_normal",
         .index = 101,
         .kind = AquaticKind::Seagrass,
         .biomes = {"ocean"},
         .bodyCount = 48,
         .tallProbability = 0.3F},
        {.name = "seagrass_deep",
         .index = 102,
         .kind = AquaticKind::Seagrass,
         .biomes = {"deep_ocean"},
         .bodyCount = 48,
         .tallProbability = 0.8F},
        {.name = "seagrass_cold",
         .index = 103,
         .kind = AquaticKind::Seagrass,
         .biomes = {"cold_ocean"},
         .bodyCount = 32,
         .tallProbability = 0.3F},
        {.name = "seagrass_deep_cold",
         .index = 104,
         .kind = AquaticKind::Seagrass,
         .biomes = {"deep_cold_ocean"},
         .bodyCount = 40,
         .tallProbability = 0.8F},
        {.name = "kelp_cold",
         .index = 105,
         .kind = AquaticKind::Kelp,
         .biomes = {"cold_ocean", "deep_cold_ocean", "deep_ocean", "ocean"},
         .noiseToCountRatio = 120}};
    return built;
}

[[nodiscard]] bool canSeagrassSurvive(const AquaticRegion& region, const DecorationPos& position, ChunkHeight bounds) {
    if (position.y <= bounds.minY()) {
        return false;
    }
    const std::int32_t below = region.get(DecorationPos{position.x, position.y - 1, position.z});
    return below != stateIdOf("magma_block") && registry::blockShapes::isFaceSturdy(below, registry::BlockFace::Up);
}

[[nodiscard]] bool canSeaPickleSurvive(const AquaticRegion& region, const DecorationPos& position, ChunkHeight bounds) {
    if (position.y <= bounds.minY()) {
        return false;
    }
    const std::int32_t below = region.get(DecorationPos{position.x, position.y - 1, position.z});
    // The boxes are flattened six numbers each; index 4 of a box is its top edge.
    const std::span<const std::int32_t> boxes = registry::blockShapes::collisionBoxes(below);
    bool hasTopCollision = false;
    for (std::size_t offset = 4; offset < boxes.size(); offset += 6) {
        if (boxes[offset] == registry::blockShapes::kUnits) {
            hasTopCollision = true;
            break;
        }
    }
    return hasTopCollision || registry::blockShapes::isFaceSturdy(below, registry::BlockFace::Up);
}

[[nodiscard]] bool canKelpSurvive(const AquaticRegion& region, const DecorationPos& position, ChunkHeight bounds) {
    if (position.y <= bounds.minY()) {
        return false;
    }
    const std::int32_t below = region.get(DecorationPos{position.x, position.y - 1, position.z});
    if (below == stateIdOf("magma_block")) {
        return false;
    }
    return below == stateIdOf("kelp_plant") || registry::blockStateFamilies::isKelpState(below) ||
           registry::blockShapes::isFaceSturdy(below, registry::BlockFace::Up);
}

void placeSeagrass(AquaticRegion& region, random::RandomSource& random, const DecorationPos& origin,
                   float tallProbability, ChunkHeight bounds) {
    const std::int32_t x = origin.x + random.nextInt(8) - random.nextInt(8);
    const std::int32_t z = origin.z + random.nextInt(8) - random.nextInt(8);
    const DecorationPos target{x, region.oceanFloor(x, z), z};
    if (!isWaterBlock(region.get(target))) {
        return;
    }
    // parity: the tall roll happens before the survival test, so a rejected position still consumes
    // its draw.
    const bool tall = random.nextDouble() < static_cast<double>(tallProbability);
    if (!canSeagrassSurvive(region, target, bounds)) {
        return;
    }
    if (!tall) {
        region.set(target, stateIdOf("seagrass"));
        return;
    }
    const DecorationPos above{target.x, target.y + 1, target.z};
    if (!isWaterBlock(region.get(above))) {
        return;
    }
    region.set(target, stateIdOf("tall_seagrass"));
    const std::optional<std::int32_t> upper =
        registry::blockStateFamilies::upperDoublePlantStateId(stateIdOf("tall_seagrass"));
    if (!upper.has_value()) {
        throw std::runtime_error{"missing tall seagrass upper half"};
    }
    region.set(above, *upper);
}

void placeSeaPickles(AquaticRegion& region, random::RandomSource& random, const DecorationPos& origin,
                     ChunkHeight bounds) {
    for (std::int32_t attempt = 0; attempt < 20; ++attempt) {
        const std::int32_t x = origin.x + random.nextInt(8) - random.nextInt(8);
        const std::int32_t z = origin.z + random.nextInt(8) - random.nextInt(8);
        const DecorationPos target{x, region.oceanFloor(x, z), z};
        const std::int32_t stateId = stateIdOf("sea_pickle") + random.nextInt(4) * 2;
        if (isWaterBlock(region.get(target)) && canSeaPickleSurvive(region, target, bounds)) {
            region.set(target, stateId);
        }
    }
}

void placeKelp(AquaticRegion& region, random::RandomSource& random, const DecorationPos& origin, ChunkHeight bounds) {
    DecorationPos position{origin.x, region.oceanFloor(origin.x, origin.z), origin.z};
    if (!isWaterBlock(region.get(position))) {
        return;
    }
    const std::int32_t height = 1 + random.nextInt(10);
    for (std::int32_t level = 0; level <= height; ++level) {
        const DecorationPos above{position.x, position.y + 1, position.z};
        if (isWaterBlock(region.get(position)) && isWaterBlock(region.get(above)) &&
            canKelpSurvive(region, position, bounds)) {
            if (level == height) {
                region.set(position, stateIdOf("kelp") + 20 + random.nextInt(4));
            } else {
                region.set(position, stateIdOf("kelp_plant"));
            }
            position = above;
            continue;
        }
        if (level <= 0) {
            return;
        }
        const DecorationPos previous{position.x, position.y - 1, position.z};
        const DecorationPos belowPrevious{previous.x, previous.y - 1, previous.z};
        if (canKelpSurvive(region, previous, bounds) &&
            !registry::blockStateFamilies::isKelpState(region.get(belowPrevious))) {
            region.set(previous, stateIdOf("kelp") + 20 + random.nextInt(4));
        }
        return;
    }
}

void decorateSpecs(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
                   const std::vector<const AquaticSpec*>& chosen, ExternalStateReader externalBlockState,
                   BlockWriteObserver observer) {
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    const PlacementContext context{chunk};
    AquaticRegion region{chunk, std::move(externalBlockState), std::move(observer)};
    const ChunkHeight bounds = chunk.verticalBounds();
    const DecorationPos chunkOrigin{chunk.chunkX() * 16, bounds.minY(), chunk.chunkZ() * 16};

    for (const AquaticSpec* spec : chosen) {
        random::setFeatureSeed(random, decorationSeed, spec->index, kVegetalDecorationStep);
        if (spec->rarity.has_value() && random.nextFloat() >= 1.0F / static_cast<float>(*spec->rarity)) {
            continue;
        }
        std::int32_t outerCount = 1;
        if (spec->noiseToCountRatio.has_value()) {
            outerCount = static_cast<std::int32_t>(
                noiseBasedCountPlacement(*spec->noiseToCountRatio, 80.0)(context, random, chunkOrigin).size());
        }
        for (std::int32_t outer = 0; outer < outerCount; ++outer) {
            const std::int32_t x = chunk.chunkX() * 16 + random.nextInt(16);
            const std::int32_t z = chunk.chunkZ() * 16 + random.nextInt(16);
            const std::int32_t y = context.height(x & 15, z & 15, HeightmapType::OceanFloor);
            if (std::ranges::find(spec->biomes, biomePath(environment.biomeAt(x, y, z))) == spec->biomes.end()) {
                continue;
            }
            const DecorationPos origin{x, y, z};
            switch (spec->kind) {
            case AquaticKind::Seagrass:
                for (std::int32_t body = 0; body < spec->bodyCount; ++body) {
                    placeSeagrass(region, random, origin, spec->tallProbability, bounds);
                }
                break;
            case AquaticKind::SeaPickle:
                placeSeaPickles(region, random, origin, bounds);
                break;
            case AquaticKind::Kelp:
                placeKelp(region, random, origin, bounds);
                break;
            }
        }
    }
}

} // namespace

void aquaticPatchDecoration::decorate(Chunk& chunk, std::int64_t seed,
                                      const surface::SurfaceRuleEnvironment& environment,
                                      ExternalStateReader externalBlockState, BlockWriteObserver observer) {
    std::vector<const AquaticSpec*> chosen;
    chosen.reserve(specs().size());
    for (const AquaticSpec& spec : specs()) {
        chosen.push_back(&spec);
    }
    decorateSpecs(chunk, seed, environment, chosen, std::move(externalBlockState), std::move(observer));
}

void aquaticPatchDecoration::decorateFeature(Chunk& chunk, std::int64_t seed,
                                             const surface::SurfaceRuleEnvironment& environment, std::string_view name,
                                             ExternalStateReader externalBlockState, BlockWriteObserver observer) {
    std::vector<const AquaticSpec*> chosen;
    for (const AquaticSpec& spec : specs()) {
        if (spec.name == name) {
            chosen.push_back(&spec);
        }
    }
    if (chosen.size() != 1) {
        throw std::invalid_argument{"unknown aquatic patch '" + std::string{name} + "'"};
    }
    decorateSpecs(chunk, seed, environment, chosen, std::move(externalBlockState), std::move(observer));
}

} // namespace basalt::server::world::terrain::decoration
