#include "AquaticSupport.hpp"

#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockStateFamilies.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/AquaticPatchDecoration.hpp>
#include <basalt/server/world/terrain/decoration/Placement.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
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
    /// parity: the tall seagrass entry's weight in the 26.3 selector, out of a total of 100.
    std::int32_t tallWeight = 0;
    std::optional<std::int32_t> rarity;
    std::optional<std::int32_t> noiseToCountRatio;
};

const std::vector<AquaticSpec>& specs() {
    static const std::vector<AquaticSpec> built = {
        {.name = "seagrass_swamp",
         .index = 90,
         .kind = AquaticKind::Seagrass,
         .biomes = {"mangrove_swamp", "swamp"},
         .bodyCount = 64,
         .tallWeight = 60},
        {.name = "seagrass_river",
         .index = 93,
         .kind = AquaticKind::Seagrass,
         .biomes = {"river"},
         .bodyCount = 48,
         .tallWeight = 40},
        {.name = "seagrass_warm",
         .index = 100,
         .kind = AquaticKind::Seagrass,
         .biomes = {"lukewarm_ocean", "warm_ocean"},
         .bodyCount = 80,
         .tallWeight = 30},
        {.name = "sea_pickle", .index = 101, .kind = AquaticKind::SeaPickle, .biomes = {"warm_ocean"}, .rarity = 16},
        {.name = "seagrass_deep_warm",
         .index = 102,
         .kind = AquaticKind::Seagrass,
         .biomes = {"deep_lukewarm_ocean"},
         .bodyCount = 80,
         .tallWeight = 80},
        {.name = "kelp_warm",
         .index = 103,
         .kind = AquaticKind::Kelp,
         .biomes = {"deep_lukewarm_ocean", "lukewarm_ocean"},
         .noiseToCountRatio = 80},
        {.name = "seagrass_normal",
         .index = 104,
         .kind = AquaticKind::Seagrass,
         .biomes = {"ocean"},
         .bodyCount = 48,
         .tallWeight = 30},
        {.name = "seagrass_deep",
         .index = 105,
         .kind = AquaticKind::Seagrass,
         .biomes = {"deep_ocean"},
         .bodyCount = 48,
         .tallWeight = 80},
        {.name = "seagrass_cold",
         .index = 106,
         .kind = AquaticKind::Seagrass,
         .biomes = {"cold_ocean"},
         .bodyCount = 32,
         .tallWeight = 30},
        {.name = "seagrass_deep_cold",
         .index = 107,
         .kind = AquaticKind::Seagrass,
         .biomes = {"deep_cold_ocean"},
         .bodyCount = 40,
         .tallWeight = 80},
        {.name = "kelp_cold",
         .index = 108,
         .kind = AquaticKind::Kelp,
         .biomes = {"cold_ocean", "deep_cold_ocean", "deep_ocean", "ocean"},
         .noiseToCountRatio = 120}};
    return built;
}

[[nodiscard]] std::int32_t below(const AquaticRegion& region, const DecorationPos& position) {
    return region.get(DecorationPos{position.x, position.y - 1, position.z});
}

[[nodiscard]] DecorationPos above(const DecorationPos& position) { return {position.x, position.y + 1, position.z}; }

/// parity: the 26.3 `offset` of the aquatic patches, a zero-width y trapezoid included: it still draws.
[[nodiscard]] DecorationPos offsetWithinSeven(random::RandomSource& random, const DecorationPos& origin) {
    const std::int32_t dx = random.nextInt(8) - random.nextInt(8);
    const std::int32_t dy = random.nextInt(1) - random.nextInt(1);
    const std::int32_t dz = random.nextInt(8) - random.nextInt(8);
    return {origin.x + dx, origin.y + dy, origin.z + dz};
}

/// parity: `HeightmapPlacement` on `OCEAN_FLOOR`, which drops a column with nothing above the floor.
[[nodiscard]] std::optional<DecorationPos> onOceanFloor(const AquaticRegion& region, const DecorationPos& position,
                                                        ChunkHeight bounds) {
    const std::int32_t y = region.oceanFloor(position.x, position.z);
    if (y <= bounds.minY()) {
        return std::nullopt;
    }
    return DecorationPos{position.x, y, position.z};
}

/// parity: `seagrass_*`, a weighted pick between a tall plant that wants water above and a short one.
void placeSeagrass(AquaticRegion& region, random::RandomSource& random, const DecorationPos& target,
                   std::int32_t tallWeight, ChunkHeight bounds) {
    const bool tall = random.nextInt(100) < tallWeight;
    const bool survives = target.y > bounds.minY() && aquaticsupport::seagrassSupport(below(region, target));
    if (!tall) {
        if (survives) {
            region.set(target, stateIdOf("seagrass"));
        }
        return;
    }
    // parity: the tall entry's own filter, then `SimpleBlockFeature`'s double plant check, which
    // wants the same fluid above, so a flowing block does not do.
    if (!isWaterBlock(region.get(above(target))) || !survives ||
        region.get(above(target)) != registry::blocks::water().stateId) {
        return;
    }
    region.set(target, stateIdOf("tall_seagrass"));
    const std::optional<std::int32_t> upper =
        registry::blockStateFamilies::upperDoublePlantStateId(stateIdOf("tall_seagrass"));
    if (!upper.has_value()) {
        throw std::runtime_error{"missing tall seagrass upper half"};
    }
    region.set(above(target), *upper);
}

/// parity: `sea_pickle`, a simple block whose pickle count is drawn before the survival test.
void placeSeaPickle(AquaticRegion& region, random::RandomSource& random, const DecorationPos& target,
                    ChunkHeight bounds) {
    const std::int32_t stateId = stateIdOf("sea_pickle") + random.nextInt(4) * 2;
    if (target.y > bounds.minY() && aquaticsupport::seaPickleSupport(below(region, target))) {
        region.set(target, stateId);
    }
}

/// parity: the kelp placement filter: water here and above, and a sturdy floor that is not magma.
[[nodiscard]] bool kelpCanStart(const AquaticRegion& region, const DecorationPos& position, ChunkHeight bounds) {
    return isWaterBlock(region.get(position)) && isWaterBlock(region.get(above(position))) &&
           position.y > bounds.minY() && aquaticsupport::seagrassSupport(below(region, position));
}

/// parity: `BlockColumnFeature` with kelp's two layers, a 0..9 stalk and one tip, truncated from the
/// stalk first where the column runs out of water two blocks up.
void placeKelp(AquaticRegion& region, random::RandomSource& random, const DecorationPos& origin) {
    std::int32_t stalk = random.nextInt(10);
    std::int32_t tip = 1;
    const std::int32_t total = stalk + tip;
    for (std::int32_t step = 0; step < total; ++step) {
        const DecorationPos next{origin.x, origin.y + 1 + step, origin.z};
        if (!isWaterBlock(region.get(next)) || !isWaterBlock(region.get(above(next)))) {
            std::int32_t remove = total - step;
            const std::int32_t fromStalk = std::min(stalk, remove);
            stalk -= fromStalk;
            remove -= fromStalk;
            tip -= std::min(tip, remove);
            break;
        }
    }
    DecorationPos position = origin;
    for (std::int32_t level = 0; level < stalk; ++level) {
        region.set(position, stateIdOf("kelp_plant"));
        position = above(position);
    }
    if (tip > 0) {
        region.set(position, stateIdOf("kelp") + 20 + random.nextInt(4));
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
        const auto inBiome = [&environment, spec](const DecorationPos& position) {
            return std::ranges::find(spec->biomes, biomePath(environment.biomeAt(position.x, position.y,
                                                                                 position.z))) != spec->biomes.end();
        };
        const auto inSquare = [&random, &chunkOrigin] {
            const std::int32_t x = chunkOrigin.x + random.nextInt(16);
            const std::int32_t z = chunkOrigin.z + random.nextInt(16);
            return DecorationPos{x, chunkOrigin.y, z};
        };
        if (spec->kind == AquaticKind::Kelp) {
            const std::int32_t count = static_cast<std::int32_t>(
                noiseBasedCountPlacement(*spec->noiseToCountRatio, 80.0)(context, random, chunkOrigin).size());
            for (std::int32_t index = 0; index < count; ++index) {
                const std::optional<DecorationPos> target = onOceanFloor(region, inSquare(), bounds);
                if (target.has_value() && kelpCanStart(region, *target, bounds) && inBiome(*target)) {
                    placeKelp(region, random, *target);
                }
            }
            continue;
        }
        const DecorationPos square = inSquare();
        const std::int32_t count = spec->kind == AquaticKind::SeaPickle ? 20 : spec->bodyCount;
        for (std::int32_t index = 0; index < count; ++index) {
            const std::optional<DecorationPos> target = onOceanFloor(region, offsetWithinSeven(random, square), bounds);
            if (!target.has_value() || !isWaterBlock(region.get(*target)) || !inBiome(*target)) {
                continue;
            }
            if (spec->kind == AquaticKind::SeaPickle) {
                placeSeaPickle(region, random, *target, bounds);
            } else {
                placeSeagrass(region, random, *target, spec->tallWeight, bounds);
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

const std::set<std::string, std::less<>>& aquaticPatchDecoration::placedIds() {
    static const std::set<std::string, std::less<>> ids = [] {
        std::set<std::string, std::less<>> all{
            "seagrass_short", "seagrass_slightly_less_short", "seagrass_mid", "seagrass_tall", "sea_pickle", "kelp"};
        for (const AquaticSpec& spec : specs()) {
            all.emplace(spec.name);
        }
        return all;
    }();
    return ids;
}

bool aquaticPatchDecoration::placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin,
                                     std::string_view name) {
    // parity: the four seagrass features are one selector with the tall entry weighted 30, 40, 60 or 80.
    static constexpr std::array<std::pair<std::string_view, std::int32_t>, 4> kSeagrass{
        {{"seagrass_short", 30}, {"seagrass_slightly_less_short", 40}, {"seagrass_mid", 60}, {"seagrass_tall", 80}}};
    AquaticKind kind = AquaticKind::Seagrass;
    std::int32_t tallWeight = 0;
    if (const auto seagrass = std::ranges::find(kSeagrass, name, &std::pair<std::string_view, std::int32_t>::first);
        seagrass != kSeagrass.end()) {
        tallWeight = seagrass->second;
    } else if (name == "kelp") {
        kind = AquaticKind::Kelp;
    } else {
        const auto spec = std::ranges::find(specs(), name, &AquaticSpec::name);
        if (spec == specs().end()) {
            return false;
        }
        kind = spec->kind;
        tallWeight = spec->tallWeight;
    }
    AquaticRegion region{chunk, nullptr, nullptr};
    const ChunkHeight bounds = chunk.verticalBounds();
    const std::int32_t before = region.get(origin);
    switch (kind) {
    case AquaticKind::Seagrass:
        placeSeagrass(region, random, origin, tallWeight, bounds);
        break;
    case AquaticKind::SeaPickle:
        placeSeaPickle(region, random, origin, bounds);
        break;
    case AquaticKind::Kelp:
        if (!kelpCanStart(region, origin, bounds)) {
            return false;
        }
        placeKelp(region, random, origin);
        break;
    }
    return region.get(origin) != before;
}

} // namespace basalt::server::world::terrain::decoration
