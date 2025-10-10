#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/CaveVineColumnFeature.hpp>
#include <basalt/server/world/terrain/decoration/LushCaveVegetationDecoration.hpp>
#include <basalt/server/world/terrain/decoration/LushVegetationPatchFeature.hpp>
#include <basalt/server/world/terrain/decoration/RootedAzaleaFeature.hpp>
#include <basalt/server/world/terrain/decoration/VinePatchDecoration.hpp>

#include <functional>
#include <optional>
#include <stdexcept>
#include <string>

namespace basalt::server::world::terrain::decoration {
namespace {

constexpr std::int32_t kVegetalDecorationStep = 9;
constexpr std::int32_t kMaxPlacementY = 256;

enum class Target : std::uint8_t { Solid, SturdyDown };

[[nodiscard]] bool matches(Target target, std::int32_t stateId) {
    return target == Target::Solid ? registry::blockShapes::isSolidState(stateId)
                                   : registry::blockShapes::isFaceSturdy(stateId, registry::BlockFace::Down);
}

struct Scan {
    std::int32_t direction;
    std::int32_t maxSteps;
    Target target;
};

struct FeatureSpec {
    std::string_view name;
    std::int32_t index;
    std::int32_t minCount;
    std::int32_t maxCount;
    std::optional<Scan> scan;
    std::int32_t yOffset;
    std::function<void(Chunk&, random::RandomSource&, const DecorationPos&)> place;
};

[[nodiscard]] std::int32_t blockAt(const Chunk& chunk, const DecorationPos& position) {
    return chunk.decorationBlockStateId(position.x, position.y, position.z);
}

[[nodiscard]] std::string_view biomePath(std::string_view id) {
    const std::size_t colon = id.find(':');
    return colon == std::string_view::npos ? id : id.substr(colon + 1);
}

void placeSporeBlossom(Chunk& chunk, const DecorationPos& origin) {
    if (!registry::blocks::isAir(blockAt(chunk, origin))) {
        return;
    }
    const std::int32_t support = blockAt(chunk, DecorationPos{origin.x, origin.y + 1, origin.z});
    if (!registry::blockShapes::isFaceSturdy(support, registry::BlockFace::Down)) {
        return;
    }
    chunk.setDecorationBlockStateId(origin.x, origin.y, origin.z, registry::blocks::named("spore_blossom")->stateId);
}

[[nodiscard]] std::optional<DecorationPos> scan(const Chunk& chunk, const DecorationPos& origin, const Scan& config) {
    DecorationPos position = origin;
    if (!registry::blocks::isAir(blockAt(chunk, position))) {
        return std::nullopt;
    }
    const ChunkHeight bounds = chunk.verticalBounds();
    for (std::int32_t step = 0; step < config.maxSteps; ++step) {
        if (matches(config.target, blockAt(chunk, position))) {
            return position;
        }
        position = DecorationPos{position.x, position.y + config.direction, position.z};
        if (position.y < bounds.minY() || position.y > bounds.maxY()) {
            return std::nullopt;
        }
        if (!registry::blocks::isAir(blockAt(chunk, position))) {
            break;
        }
    }
    return matches(config.target, blockAt(chunk, position)) ? std::optional<DecorationPos>{position} : std::nullopt;
}

const std::vector<FeatureSpec>& specs() {
    static const std::vector<FeatureSpec> built = [] {
        constexpr Scan upSolid{1, 12, Target::Solid};
        constexpr Scan upSturdy{1, 12, Target::SturdyDown};
        constexpr Scan downSolid{-1, 12, Target::Solid};
        std::vector<FeatureSpec> specs;
        specs.push_back({"lush_caves_ceiling_vegetation", 27, 125, 125, upSolid, -1,
                         [](Chunk& chunk, random::RandomSource& random, const DecorationPos& origin) {
                             static_cast<void>(lushVegetationPatchFeature::placeCeilingMoss(chunk, random, origin));
                         }});
        specs.push_back({"cave_vines", 28, 188, 188, upSturdy, -1,
                         [](Chunk& chunk, random::RandomSource& random, const DecorationPos& origin) {
                             static_cast<void>(caveVineColumnFeature::place(chunk, random, origin));
                         }});
        specs.push_back({"lush_caves_clay", 29, 62, 62, downSolid, 1,
                         [](Chunk& chunk, random::RandomSource& random, const DecorationPos& origin) {
                             static_cast<void>(lushVegetationPatchFeature::placeClay(chunk, random, origin));
                         }});
        specs.push_back({"lush_caves_vegetation", 30, 125, 125, downSolid, 1,
                         [](Chunk& chunk, random::RandomSource& random, const DecorationPos& origin) {
                             static_cast<void>(lushVegetationPatchFeature::placeFloorMoss(chunk, random, origin));
                         }});
        specs.push_back({"rooted_azalea_tree", 31, 1, 2, upSolid, -1,
                         [](Chunk& chunk, random::RandomSource& random, const DecorationPos& origin) {
                             static_cast<void>(rootedAzaleaFeature::place(chunk, random, origin));
                         }});
        specs.push_back({"spore_blossom", 32, 25, 25, upSolid, -1,
                         [](Chunk& chunk, random::RandomSource&, const DecorationPos& origin) {
                             placeSporeBlossom(chunk, origin);
                         }});
        specs.push_back({"classic_vines_cave_feature", 33, 256, 256, std::nullopt, 0,
                         [](Chunk& chunk, random::RandomSource&, const DecorationPos& origin) {
                             vinePatchDecoration::placeCaveAt(chunk, origin);
                         }});
        return specs;
    }();
    return built;
}

void decorateSpecs(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
                   const std::vector<const FeatureSpec*>& selected) {
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    const std::int32_t minY = chunk.verticalBounds().minY();
    for (const FeatureSpec* spec : selected) {
        random::setFeatureSeed(random, decorationSeed, spec->index, kVegetalDecorationStep);
        const std::int32_t count = spec->minCount == spec->maxCount
                                       ? spec->minCount
                                       : spec->minCount + random.nextInt(spec->maxCount - spec->minCount + 1);
        for (std::int32_t attempt = 0; attempt < count; ++attempt) {
            const std::int32_t x = chunk.chunkX() * 16 + random.nextInt(16);
            const std::int32_t z = chunk.chunkZ() * 16 + random.nextInt(16);
            const std::int32_t y = minY + random.nextInt(kMaxPlacementY - minY + 1);
            const DecorationPos position{x, y, z};
            DecorationPos scanned = position;
            if (spec->scan.has_value()) {
                const std::optional<DecorationPos> found = scan(chunk, position, *spec->scan);
                if (!found.has_value()) {
                    continue;
                }
                scanned = *found;
            }
            const DecorationPos origin{scanned.x, scanned.y + spec->yOffset, scanned.z};
            if (biomePath(environment.biomeAt(origin.x, origin.y, origin.z)) != "lush_caves") {
                continue;
            }
            spec->place(chunk, random, origin);
        }
    }
}

} // namespace

const std::vector<std::string_view>& lushCaveVegetationDecoration::featureNames() {
    static const std::vector<std::string_view> names = [] {
        std::vector<std::string_view> built;
        built.reserve(specs().size());
        for (const FeatureSpec& spec : specs()) {
            built.push_back(spec.name);
        }
        return built;
    }();
    return names;
}

void lushCaveVegetationDecoration::decorate(Chunk& chunk, const std::int64_t seed,
                                            const surface::SurfaceRuleEnvironment& environment) {
    std::vector<const FeatureSpec*> selected;
    selected.reserve(specs().size());
    for (const FeatureSpec& spec : specs()) {
        selected.push_back(&spec);
    }
    decorateSpecs(chunk, seed, environment, selected);
}

void lushCaveVegetationDecoration::decorateFeature(Chunk& chunk, const std::int64_t seed,
                                                   const surface::SurfaceRuleEnvironment& environment,
                                                   const std::string_view name) {
    for (const FeatureSpec& spec : specs()) {
        if (spec.name == name) {
            decorateSpecs(chunk, seed, environment, {&spec});
            return;
        }
    }
    throw std::runtime_error{"unknown lush cave feature '" + std::string{name} + "'"};
}

} // namespace basalt::server::world::terrain::decoration
