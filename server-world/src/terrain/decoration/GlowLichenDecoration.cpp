#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockStateFamilies.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/GlowLichenDecoration.hpp>
#include <basalt/server/world/terrain/decoration/PatchRegion.hpp>
#include <basalt/server/world/terrain/decoration/Placement.hpp>

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace basalt::server::world::terrain::decoration {
namespace {

constexpr std::int32_t kWaterloggedFalseBit = 2;
constexpr std::int32_t kAllFaceFalseBits = 64 + 32 + 16 + 8 + 4 + 1;
constexpr float kSpreadChance = 0.5F;
constexpr std::int32_t kGlowLichenIndex = 0;
constexpr std::int32_t kVegetalDecorationStep = 9;

enum class Axis : std::uint8_t { X, Y, Z };

struct Direction {
    std::int32_t dx;
    std::int32_t dy;
    std::int32_t dz;
    Axis axis;
    /// The bit cleared from the default state to turn that face on: vanilla's lichen states count
    /// down from all-faces-set, exactly as vines do.
    std::int32_t falseBit;
    registry::BlockFace blockFace;

    friend bool operator==(const Direction&, const Direction&) = default;
};

constexpr Direction kDown{0, -1, 0, Axis::Y, 64, registry::BlockFace::Down};
constexpr Direction kUp{0, 1, 0, Axis::Y, 4, registry::BlockFace::Up};
constexpr Direction kNorth{0, 0, -1, Axis::Z, 16, registry::BlockFace::North};
constexpr Direction kSouth{0, 0, 1, Axis::Z, 8, registry::BlockFace::South};
constexpr Direction kWest{-1, 0, 0, Axis::X, 1, registry::BlockFace::West};
constexpr Direction kEast{1, 0, 0, Axis::X, 32, registry::BlockFace::East};

[[nodiscard]] Direction opposite(const Direction& direction) {
    if (direction == kDown) {
        return kUp;
    }
    if (direction == kUp) {
        return kDown;
    }
    if (direction == kNorth) {
        return kSouth;
    }
    if (direction == kSouth) {
        return kNorth;
    }
    if (direction == kWest) {
        return kEast;
    }
    return kWest;
}

constexpr std::array<Direction, 5> kValidDirections = {kUp, kNorth, kEast, kSouth, kWest};
constexpr std::array<Direction, 6> kAllDirections = {kDown, kUp, kNorth, kSouth, kWest, kEast};

[[nodiscard]] std::int32_t stateIdOf(std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    if (found == nullptr) {
        throw std::runtime_error{"unknown block '" + std::string{path} + "'"};
    }
    return found->stateId;
}

[[nodiscard]] std::int32_t firstGlowLichenStateId() { return stateIdOf("glow_lichen") - 127; }

const std::set<std::int32_t>& canBePlacedOnStateIds() {
    static const std::set<std::int32_t> ids = [] {
        std::set<std::int32_t> built;
        for (const std::string_view path : {"stone", "andesite", "diorite", "granite", "dripstone_block", "calcite",
                                            "tuff", "deepslate", "sulfur", "cinnabar"}) {
            built.insert(stateIdOf(path));
        }
        return built;
    }();
    return ids;
}

[[nodiscard]] bool isWaterBlock(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block != nullptr && block->path() == "water";
}

[[nodiscard]] bool isAirWaterOrLichen(std::int32_t stateId) {
    return registry::blocks::isAir(stateId) || isWaterBlock(stateId) ||
           registry::blockStateFamilies::isGlowLichenState(stateId);
}

/// parity: `MultifaceSpreader.DefaultSpreaderConfig.stateCanBeReplaced`. The spread step is stricter
/// than the first placement: it takes water only where it is a source, while an origin or a search
/// target takes any water level.
[[nodiscard]] bool spreadCanReplace(std::int32_t stateId) {
    if (registry::blocks::isAir(stateId) || registry::blockStateFamilies::isGlowLichenState(stateId)) {
        return true;
    }
    return isWaterBlock(stateId) && stateId == registry::blocks::water().stateId;
}

/// Source water only: a flowing level would waterlog the lichen with the wrong fluid state.
[[nodiscard]] bool hasSourceWaterFluid(std::int32_t stateId) {
    const std::int32_t water = registry::blocks::water().stateId;
    return registry::blockFluids::hasWaterFluid(stateId) && !(stateId > water && stateId <= water + 15);
}

[[nodiscard]] std::optional<std::int32_t> withFace(std::int32_t oldState, const Direction& direction) {
    std::int32_t bits = 0;
    if (registry::blockStateFamilies::isGlowLichenState(oldState)) {
        bits = oldState - firstGlowLichenStateId();
    } else if (hasSourceWaterFluid(oldState)) {
        bits = kAllFaceFalseBits;
    } else if (registry::blocks::isAir(oldState) || isWaterBlock(oldState)) {
        bits = kAllFaceFalseBits | kWaterloggedFalseBit;
    } else {
        return std::nullopt;
    }
    if ((bits & direction.falseBit) == 0) {
        return std::nullopt;
    }
    bits &= ~direction.falseBit;
    return firstGlowLichenStateId() + bits;
}

[[nodiscard]] bool hasFace(std::int32_t stateId, const Direction& direction) {
    return registry::blockStateFamilies::isGlowLichenState(stateId) &&
           ((stateId - firstGlowLichenStateId()) & direction.falseBit) == 0;
}

/// parity: `Util.shuffle`, a Fisher-Yates walking downward. The draw count and order are observable.
template <typename T>
[[nodiscard]] std::vector<T> shuffled(const std::vector<T>& source, random::RandomSource& random) {
    std::vector<T> copy = source;
    for (std::size_t size = copy.size(); size >= 2; --size) {
        const auto swapTo = static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(size)));
        std::swap(copy[size - 1], copy[swapTo]);
    }
    return copy;
}

[[nodiscard]] DecorationPos relative(const DecorationPos& position, const Direction& direction) {
    return DecorationPos{position.x + direction.dx, position.y + direction.dy, position.z + direction.dz};
}

/// The patch region plus the sturdy-face question the spread step asks.
class LichenRegion {
public:
    LichenRegion(Chunk& chunk, ExternalStateReader externalBlockState)
        : chunk_(&chunk), external_(externalBlockState), inner_(chunk, std::move(externalBlockState), nullptr) {}

    /// Nothing rather than air outside the chunk with no reader: a growth cannot judge a face it
    /// cannot see, and answering air would attach lichen to phantom walls at every border.
    [[nodiscard]] std::optional<std::int32_t> get(const DecorationPos& position) const {
        const ChunkHeight bounds = chunk_->verticalBounds();
        if (position.y < bounds.minY() || position.y > bounds.maxY()) {
            return registry::blocks::air().stateId;
        }
        if ((position.x >> 4) == chunk_->chunkX() && (position.z >> 4) == chunk_->chunkZ()) {
            return chunk_->blockStateId(position.x & 15, position.y, position.z & 15);
        }
        // parity: same as every other region, the chunk's own queue for the neighbour comes first.
        if (const std::optional<std::int32_t> queued =
                chunk_->queuedDecorationStateId(position.x, position.y, position.z);
            queued.has_value()) {
            return queued;
        }
        if (external_) {
            return external_(position.x, position.y, position.z);
        }
        return std::nullopt;
    }

    void set(const DecorationPos& position, std::int32_t stateId) {
        const ChunkHeight bounds = chunk_->verticalBounds();
        if (position.y < bounds.minY() || position.y > bounds.maxY()) {
            return;
        }
        inner_.set(position, stateId);
    }

    [[nodiscard]] bool hasSturdyFace(const DecorationPos& position, const Direction& face) const {
        const std::optional<std::int32_t> stateId = get(position);
        if (!stateId.has_value()) {
            return false;
        }
        return registry::blockShapes::isFaceSturdy(*stateId, face.blockFace) ||
               registry::blockShapes::faceCovers(*stateId, face.blockFace, 0, registry::blockShapes::kUnits, 0,
                                                 registry::blockShapes::kUnits);
    }

private:
    Chunk* chunk_;
    ExternalStateReader external_;
    PatchRegion inner_;
};

void spread(LichenRegion& region, random::RandomSource& random, const DecorationPos& source, std::int32_t sourceState,
            const Direction& startingFace) {
    const std::vector<Direction> all{kAllDirections.begin(), kAllDirections.end()};
    for (const Direction& spreadDirection : shuffled(all, random)) {
        if (spreadDirection.axis == startingFace.axis || hasFace(sourceState, spreadDirection)) {
            continue;
        }
        const std::array<std::pair<DecorationPos, Direction>, 3> candidates = {
            std::pair{source, spreadDirection}, std::pair{relative(source, spreadDirection), startingFace},
            std::pair{relative(relative(source, spreadDirection), startingFace), opposite(spreadDirection)}};
        for (const auto& [position, face] : candidates) {
            const std::optional<std::int32_t> oldState = region.get(position);
            if (!oldState.has_value() || !spreadCanReplace(*oldState)) {
                continue;
            }
            // After the first placement vanilla attaches by the generic sturdy-face test, not by the
            // narrower can-be-placed-on list the configuration carries.
            if (!region.hasSturdyFace(relative(position, face), opposite(face))) {
                continue;
            }
            const std::optional<std::int32_t> newState = withFace(*oldState, face);
            if (!newState.has_value()) {
                continue;
            }
            region.set(position, *newState);
            return;
        }
    }
}

[[nodiscard]] bool placeOnFirstSupport(LichenRegion& region, random::RandomSource& random,
                                       const DecorationPos& position, std::int32_t oldState,
                                       const std::vector<Direction>& directions) {
    for (const Direction& direction : directions) {
        const std::optional<std::int32_t> neighbour = region.get(relative(position, direction));
        if (!neighbour.has_value() || !canBePlacedOnStateIds().contains(*neighbour)) {
            continue;
        }
        const std::optional<std::int32_t> newState = withFace(oldState, direction);
        if (!newState.has_value()) {
            return false;
        }
        region.set(position, *newState);
        if (random.nextFloat() < kSpreadChance) {
            spread(region, random, position, *newState, direction);
        }
        return true;
    }
    return false;
}

bool placeGrowth(LichenRegion& region, random::RandomSource& random, const DecorationPos& origin) {
    const std::optional<std::int32_t> oldState = region.get(origin);
    if (!oldState.has_value()) {
        return false;
    }
    // The origin gate is narrower than the search-target gate: an existing lichen may be traversed
    // during the directional search, but cannot itself be the origin.
    if (!registry::blocks::isAir(*oldState) && !isWaterBlock(*oldState)) {
        return false;
    }

    const std::vector<Direction> valid{kValidDirections.begin(), kValidDirections.end()};
    const std::vector<Direction> directions = shuffled(valid, random);
    if (placeOnFirstSupport(region, random, origin, *oldState, directions)) {
        return true;
    }

    for (const Direction& searchDirection : directions) {
        std::vector<Direction> placementSource;
        for (const Direction& candidate : kValidDirections) {
            if (!(candidate == opposite(searchDirection))) {
                placementSource.push_back(candidate);
            }
        }
        const std::vector<Direction> placementDirections = shuffled(placementSource, random);
        const DecorationPos target = relative(origin, searchDirection);
        const std::optional<std::int32_t> targetState = region.get(target);
        if (!targetState.has_value() || !isAirWaterOrLichen(*targetState)) {
            continue;
        }
        // parity: 26.2 repeats this same one-block target `searchRange` times. A failed attempt
        // consumes no random, so one evaluation is equivalent and preserves the vanilla bug.
        if (placeOnFirstSupport(region, random, target, *targetState, placementDirections)) {
            return true;
        }
    }
    return false;
}

} // namespace

bool glowLichenDecoration::placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin) {
    LichenRegion region{chunk, nullptr};
    return placeGrowth(region, random, origin);
}

void glowLichenDecoration::decorate(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
                                    ExternalStateReader externalBlockState) {
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    random::setFeatureSeed(random, decorationSeed, kGlowLichenIndex, kVegetalDecorationStep);

    const ChunkHeight bounds = chunk.verticalBounds();
    LichenRegion region{chunk, std::move(externalBlockState)};
    const DecorationPos chunkOrigin{chunk.chunkX() * 16, 0, chunk.chunkZ() * 16};
    placeFrom(
        chunk, random, chunkOrigin,
        {countPlacement(IntProvider::uniform(104, 157)), uniformHeightRangePlacement(bounds.minY(), 256),
         inSquarePlacement(),
         surfaceRelativeThresholdFilter(HeightmapType::OceanFloor, std::numeric_limits<std::int32_t>::min(), -13)},
        [&](const DecorationPos& position) {
            const std::string& biome = environment.biomeAt(position.x, position.y, position.z);
            const std::size_t colon = biome.find(':');
            if ((colon == std::string::npos ? biome : biome.substr(colon + 1)) == "the_void") {
                return;
            }
            placeGrowth(region, random, position);
        });
}

} // namespace basalt::server::world::terrain::decoration
