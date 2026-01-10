#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/BambooDecoration.hpp>

#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace basalt::server::world::terrain::decoration {
namespace {

constexpr std::int32_t kBambooIndex = 2;
constexpr std::int32_t kBambooLightIndex = 6;
constexpr std::int32_t kVegetalDecorationStep = 9;

using PathSet = std::set<std::string, std::less<>>;

const PathSet& podzolReplaceable() {
    static const PathSet paths = {"dirt",       "coarse_dirt",     "rooted_dirt", "mud",    "muddy_mangrove_roots",
                                  "moss_block", "pale_moss_block", "grass_block", "podzol", "mycelium"};
    return paths;
}

const PathSet& supportsBamboo() {
    static const PathSet paths = [] {
        PathSet built = podzolReplaceable();
        built.insert(
            {"sand", "red_sand", "suspicious_sand", "bamboo", "bamboo_sapling", "gravel", "suspicious_gravel"});
        return built;
    }();
    return paths;
}

[[nodiscard]] std::int32_t bambooBase() { return registry::blocks::named("bamboo")->stateId; }

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : block->path();
}

[[nodiscard]] std::string_view biomePath(std::string_view id) {
    const std::size_t colon = id.find(':');
    return colon == std::string_view::npos ? id : id.substr(colon + 1);
}

const std::vector<PlacementModifier>& lightModifiers() {
    static const std::vector<PlacementModifier> modifiers = {rarityFilter(4), inSquarePlacement(),
                                                             heightmapPlacement()};
    return modifiers;
}

const std::vector<PlacementModifier>& denseModifiers() {
    static const std::vector<PlacementModifier> modifiers = {
        noiseBasedCountPlacement(160, 80.0, 0.3), inSquarePlacement(), heightmapPlacement(HeightmapType::WorldSurface)};
    return modifiers;
}

/// Reads and writes with the world surface kept up to date, because a clump raises the ground it
/// then checks for podzol.
class BambooRegion {
public:
    BambooRegion(Chunk& chunk, ExternalStateReader externalBlockState)
        : chunk_(&chunk), externalBlockState_(std::move(externalBlockState)),
          localWorldSurfaceHeights_(chunk.heightmap(HeightmapType::WorldSurface)) {}

    [[nodiscard]] std::optional<std::int32_t> get(const DecorationPos& position) const {
        const ChunkHeight bounds = chunk_->verticalBounds();
        if (position.y < bounds.minY() || position.y > bounds.maxY()) {
            return registry::blocks::air().stateId;
        }
        if ((position.x >> 4) == chunk_->chunkX() && (position.z >> 4) == chunk_->chunkZ()) {
            return chunk_->blockStateId(position.x & 15, position.y, position.z & 15);
        }
        // Outside the chunk this used to read as air, so anything needing a block beneath it was
        // refused everywhere but its own chunk. Falling through to the chunk reaches the phase-wide
        // neighbour reader, which is what vanilla's own level lookup sees.
        for (const auto& [written, stateId] : externalWrites_) {
            if (written == position) {
                return stateId;
            }
        }
        if (externalBlockState_) {
            if (const std::optional<std::int32_t> observed = externalBlockState_(position.x, position.y, position.z);
                observed.has_value()) {
                return observed;
            }
        }
        return chunk_->decorationBlockStateId(position.x, position.y, position.z);
    }

    void set(const DecorationPos& position, std::int32_t stateId) {
        const ChunkHeight bounds = chunk_->verticalBounds();
        if (position.y < bounds.minY() || position.y > bounds.maxY()) {
            return;
        }
        if ((position.x >> 4) == chunk_->chunkX() && (position.z >> 4) == chunk_->chunkZ()) {
            chunk_->setBlockStateId(position.x & 15, position.y, position.z & 15, stateId);
            const std::size_t column = static_cast<std::size_t>((position.x & 15) + (position.z & 15) * 16);
            if (!registry::blocks::isAir(stateId) && position.y + 1 > localWorldSurfaceHeights_[column]) {
                localWorldSurfaceHeights_[column] = position.y + 1;
            }
            return;
        }
        setExternalDecorationBlockStateId(*chunk_, externalWrites_, externalBlockState_, position, stateId);
        const std::pair<std::int32_t, std::int32_t> column{position.x, position.z};
        const auto found = externalWorldSurfaceHeights_.find(column);
        const std::int32_t currentHeight = found != externalWorldSurfaceHeights_.end()
                                               ? found->second
                                               : scanExternalWorldSurface(position.x, position.z);
        if (!registry::blocks::isAir(stateId) && position.y + 1 > currentHeight) {
            externalWorldSurfaceHeights_[column] = position.y + 1;
        } else if (position.y + 1 == currentHeight && registry::blocks::isAir(stateId)) {
            externalWorldSurfaceHeights_[column] = scanExternalWorldSurface(position.x, position.z);
        } else {
            externalWorldSurfaceHeights_[column] = currentHeight;
        }
    }

    [[nodiscard]] std::int32_t worldSurfaceHeight(std::int32_t x, std::int32_t z) {
        if ((x >> 4) == chunk_->chunkX() && (z >> 4) == chunk_->chunkZ()) {
            return localWorldSurfaceHeights_[static_cast<std::size_t>((x & 15) + (z & 15) * 16)];
        }
        const std::pair<std::int32_t, std::int32_t> column{x, z};
        const auto found = externalWorldSurfaceHeights_.find(column);
        if (found != externalWorldSurfaceHeights_.end()) {
            return found->second;
        }
        const std::int32_t scanned = scanExternalWorldSurface(x, z);
        externalWorldSurfaceHeights_[column] = scanned;
        return scanned;
    }

    [[nodiscard]] bool inWorld(const DecorationPos& position) const {
        const ChunkHeight bounds = chunk_->verticalBounds();
        return position.y >= bounds.minY() && position.y <= bounds.maxY();
    }

private:
    [[nodiscard]] std::int32_t scanExternalWorldSurface(std::int32_t x, std::int32_t z) const {
        const ChunkHeight bounds = chunk_->verticalBounds();
        for (std::int32_t y = bounds.maxY(); y >= bounds.minY(); --y) {
            const DecorationPos position{x, y, z};
            std::int32_t stateId = registry::blocks::air().stateId;
            bool found = false;
            for (const auto& [written, written_state] : externalWrites_) {
                if (written == position) {
                    stateId = written_state;
                    found = true;
                    break;
                }
            }
            if (!found && externalBlockState_) {
                if (const std::optional<std::int32_t> observed = externalBlockState_(x, y, z); observed.has_value()) {
                    stateId = *observed;
                }
            }
            if (!registry::blocks::isAir(stateId)) {
                return y + 1;
            }
        }
        return bounds.minY();
    }

    Chunk* chunk_;
    ExternalStateReader externalBlockState_;
    std::vector<std::pair<DecorationPos, std::int32_t>> externalWrites_;
    std::vector<std::int32_t> localWorldSurfaceHeights_;
    std::map<std::pair<std::int32_t, std::int32_t>, std::int32_t> externalWorldSurfaceHeights_;
};

[[nodiscard]] bool canBambooSurvive(const BambooRegion& region, const DecorationPos& origin, ChunkHeight bounds) {
    if (origin.y <= bounds.minY()) {
        return false;
    }
    const std::optional<std::int32_t> below = region.get(DecorationPos{origin.x, origin.y - 1, origin.z});
    return below.has_value() && supportsBamboo().contains(pathOf(*below));
}

void place(BambooRegion& region, random::RandomSource& random, const DecorationPos& origin, float podzolProbability,
           ChunkHeight bounds) {
    if (!region.inWorld(origin)) {
        return;
    }
    const std::optional<std::int32_t> at = region.get(origin);
    if (!at.has_value() || !registry::blocks::isAir(*at) || !canBambooSurvive(region, origin, bounds)) {
        return;
    }

    const std::int32_t height = random.nextInt(12) + 5;
    if (random.nextFloat() < podzolProbability) {
        const std::int32_t radius = random.nextInt(4) + 1;
        for (std::int32_t x = origin.x - radius; x <= origin.x + radius; ++x) {
            for (std::int32_t z = origin.z - radius; z <= origin.z + radius; ++z) {
                const std::int32_t dx = x - origin.x;
                const std::int32_t dz = z - origin.z;
                if (dx * dx + dz * dz > radius * radius) {
                    continue;
                }
                const DecorationPos surface{x, region.worldSurfaceHeight(x, z) - 1, z};
                if (!region.inWorld(surface)) {
                    continue;
                }
                const std::optional<std::int32_t> current = region.get(surface);
                if (current.has_value() && podzolReplaceable().contains(pathOf(*current))) {
                    region.set(surface, registry::blocks::named("podzol")->stateId);
                }
            }
        }
    }

    DecorationPos position = origin;
    std::int32_t placed = 0;
    while (placed < height && region.inWorld(position)) {
        const std::optional<std::int32_t> current = region.get(position);
        if (!current.has_value() || !registry::blocks::isAir(*current)) {
            break;
        }
        region.set(position, bambooBase() + 6);
        position = DecorationPos{position.x, position.y + 1, position.z};
        ++placed;
    }
    if (placed >= 3 && region.inWorld(position)) {
        region.set(position, bambooBase() + 11);
        region.set(DecorationPos{position.x, position.y - 1, position.z}, bambooBase() + 10);
        region.set(DecorationPos{position.x, position.y - 2, position.z}, bambooBase() + 8);
    }
}

void decorateWith(Chunk& chunk, BambooRegion& region, random::RandomSource& random,
                  const std::vector<PlacementModifier>& modifiers, float podzolProbability,
                  const surface::SurfaceRuleEnvironment* environment, std::string_view requiredBiome) {
    const ChunkHeight bounds = chunk.verticalBounds();
    const DecorationPos chunkOrigin{.x = chunk.chunkX() * 16, .y = 0, .z = chunk.chunkZ() * 16};
    placeFrom(chunk, random, chunkOrigin, modifiers, [&](const DecorationPos& origin) {
        if (!requiredBiome.empty() && (environment == nullptr || biomePath(environment->biomeAt(
                                                                     origin.x, origin.y, origin.z)) != requiredBiome)) {
            return;
        }
        place(region, random, origin, podzolProbability, bounds);
    });
}

[[nodiscard]] BiomeVegetalDecoration feature(float podzolProbability, const std::vector<PlacementModifier>& modifiers) {
    return [podzolProbability, &modifiers](Chunk& chunk, random::RandomSource& random) {
        BambooRegion region{chunk, nullptr};
        decorateWith(chunk, region, random, modifiers, podzolProbability, nullptr, {});
    };
}

} // namespace

const std::set<std::string, std::less<>>& bambooDecoration::placedIds() {
    static const std::set<std::string, std::less<>> ids = {"bamboo", "bamboo_light", "bamboo_vegetation"};
    return ids;
}

BiomeVegetalDecoration bambooDecoration::light() { return feature(0.0F, lightModifiers()); }

BiomeVegetalDecoration bambooDecoration::dense() { return feature(0.2F, denseModifiers()); }

bool bambooDecoration::placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin,
                               const std::string_view placedFeature) {
    // parity: `bamboo_light` places no podzol under it, the dense one a fifth of the time.
    const float podzol = placedFeature == "bamboo_light" ? 0.0F : 0.2F;
    if (!placedIds().contains(placedFeature)) {
        return false;
    }
    BambooRegion region{chunk, nullptr};
    place(region, random, origin, podzol, chunk.verticalBounds());
    return true;
}

void bambooDecoration::decorateFeature(const std::int32_t index, Chunk& chunk, const std::int64_t seed,
                                       const surface::SurfaceRuleEnvironment& environment,
                                       ExternalStateReader externalBlockState) {
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    BambooRegion region{chunk, std::move(externalBlockState)};
    random::setFeatureSeed(random, decorationSeed, index, kVegetalDecorationStep);
    if (index == kBambooIndex) {
        decorateWith(chunk, region, random, denseModifiers(), 0.2F, &environment, "bamboo_jungle");
    } else if (index == kBambooLightIndex) {
        decorateWith(chunk, region, random, lightModifiers(), 0.0F, &environment, "jungle");
    } else {
        throw std::runtime_error{"unknown bamboo vegetal index"};
    }
}

void bambooDecoration::decorate(Chunk& chunk, const std::int64_t seed,
                                const surface::SurfaceRuleEnvironment& environment,
                                ExternalStateReader externalBlockState) {
    decorateFeature(kBambooIndex, chunk, seed, environment, externalBlockState);
    decorateFeature(kBambooLightIndex, chunk, seed, environment, std::move(externalBlockState));
}

} // namespace basalt::server::world::terrain::decoration
