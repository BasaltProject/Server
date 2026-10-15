#include "SculkShared.hpp"

#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/world/terrain/decoration/SculkDecoration.hpp>
#include <basalt/wire/common/JavaCast.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>

namespace basalt::server::world::terrain::decoration {
namespace {

using sculk::Direction;
using sculk::kDirections;
using sculk::offsetBy;
using sculk::opposite;
using sculk::SculkBehaviour;
using sculk::SculkRegion;

constexpr std::int32_t kUndergroundDecorationStep = 7;
constexpr std::int32_t kSculkVeinIndex = 0;
constexpr std::int32_t kSculkPatchIndex = 1;

/// parity: the eighteen neighbours of a block that share a face or an edge, in vanilla's own order.
const std::vector<DecorationPos>& nonCornerNeighbours() {
    static const std::vector<DecorationPos> offsets = [] {
        std::vector<DecorationPos> built;
        for (std::int32_t z = -1; z <= 1; ++z) {
            for (std::int32_t y = -1; y <= 1; ++y) {
                for (std::int32_t x = -1; x <= 1; ++x) {
                    if ((x == 0 || y == 0 || z == 0) && (x != 0 || y != 0 || z != 0)) {
                        built.push_back(DecorationPos{.x = x, .y = y, .z = z});
                    }
                }
            }
        }
        return built;
    }();
    return offsets;
}

[[nodiscard]] DecorationPos offsetBy(const DecorationPos& position, const DecorationPos& delta) {
    return DecorationPos{.x = position.x + delta.x, .y = position.y + delta.y, .z = position.z + delta.z};
}

[[nodiscard]] std::string_view biomePath(std::string_view id) {
    const std::size_t colon = id.find(':');
    return colon == std::string_view::npos ? id : id.substr(colon + 1);
}

bool regrowVein(SculkRegion& region, const DecorationPos& position, const std::int32_t existingStateId,
                const std::vector<const Direction*>& facings) {
    std::int32_t newStateId =
        sculk::hasSourceWater(existingStateId) ? sculk::states().veinWaterlogged : sculk::states().vein;
    bool placedFace = false;
    for (const Direction* face : facings) {
        if (!region.hasSturdyFace(offsetBy(position, *face), opposite(*face))) {
            continue;
        }
        const std::optional<std::int32_t> withFace = region.withFace(newStateId, position, *face);
        if (!withFace.has_value()) {
            continue;
        }
        newStateId = *withFace;
        placedFace = true;
    }
    if (placedFace) {
        region.set(position, newStateId);
    }
    return placedFace;
}

bool attemptSpreadVein(SculkRegion& region, const DecorationPos& position, const std::int32_t stateId,
                       const std::optional<std::vector<const Direction*>>& facings, const SculkBehaviour behaviour) {
    if (behaviour != SculkBehaviour::Default) {
        return sculk::spreadAll(region, stateId, position, false) > 0;
    }
    if (!facings.has_value()) {
        return sculk::spreadAll(region, stateId, position, true) > 0;
    }
    if (!facings->empty()) {
        return SculkRegion::isAirOrWaterFluid(stateId) && regrowVein(region, position, stateId, *facings);
    }
    return sculk::spreadAll(region, stateId, position, false) > 0;
}

bool attemptPlaceSculk(SculkRegion& region, const DecorationPos& position, random::RandomSource& random) {
    const std::optional<std::int32_t> stateId = region.get(position);
    if (!stateId.has_value()) {
        return false;
    }
    std::vector<const Direction*> supports;
    supports.reserve(kDirections.size());
    for (const Direction& direction : kDirections) {
        supports.push_back(&direction);
    }
    sculk::shuffle(supports, random);
    for (const Direction* support : supports) {
        if (!SculkRegion::hasFace(*stateId, *support)) {
            continue;
        }
        const DecorationPos supportPosition = offsetBy(position, *support);
        if (!sculk::replaceableWorldGen().contains(region.blockPath(supportPosition))) {
            continue;
        }
        region.set(supportPosition, sculk::states().sculk);
        sculk::spreadAll(region, sculk::states().sculk, supportPosition, false);

        const Direction& skip = opposite(*support);
        for (const Direction& direction : kDirections) {
            if (direction == skip) {
                continue;
            }
            const DecorationPos veinPosition = offsetBy(supportPosition, direction);
            const std::optional<std::int32_t> veinStateId = region.get(veinPosition);
            if (veinStateId.has_value() && SculkRegion::isVein(*veinStateId)) {
                sculk::dischargeVein(region, veinPosition, *veinStateId);
            }
        }
        return true;
    }
    return false;
}

[[nodiscard]] bool canPlaceGrowth(const SculkRegion& region, const DecorationPos& position) {
    const std::optional<std::int32_t> aboveStateId = region.get(offsetBy(position, kDirections[1]));
    if (!aboveStateId.has_value()) {
        return false;
    }
    if (!registry::blocks::isAir(*aboveStateId) && !sculk::isWaterBlock(*aboveStateId)) {
        return false;
    }
    std::int32_t growthCount = 0;
    for (std::int32_t z = position.z - 4; z <= position.z + 4; ++z) {
        for (std::int32_t y = position.y; y <= position.y + 2; ++y) {
            for (std::int32_t x = position.x - 4; x <= position.x + 4; ++x) {
                const std::string path = region.blockPath(DecorationPos{.x = x, .y = y, .z = z});
                if ((path == "sculk_sensor" || path == "sculk_shrieker") && ++growthCount > 2) {
                    return false;
                }
            }
        }
    }
    return true;
}

[[nodiscard]] std::int32_t decayPenalty(const DecorationPos& position, const DecorationPos& origin,
                                        const std::int32_t charge) {
    const std::int32_t dx = position.x - origin.x;
    const std::int32_t dy = position.y - origin.y;
    const std::int32_t dz = position.z - origin.z;
    const auto distance = static_cast<float>(std::sqrt(static_cast<double>(dx * dx + dy * dy + dz * dz)));
    const float outsideRadius = distance - 1.0F;
    const float distanceFactor = std::min(1.0F, outsideRadius * outsideRadius / 529.0F);
    return std::max(
        1, wire::common::toIntLikeJava(static_cast<double>(static_cast<float>(charge) * distanceFactor * 0.5F)));
}

[[nodiscard]] bool isUnobstructed(const SculkRegion& region, const DecorationPos& from, const Direction& direction) {
    return !region.hasSturdyFace(offsetBy(from, direction), opposite(direction));
}

[[nodiscard]] bool isMovementUnobstructed(const SculkRegion& region, const DecorationPos& from, const DecorationPos& to,
                                          const DecorationPos& delta) {
    if (sculk::manhattanDistance(from, to) == 1) {
        return true;
    }
    const Direction& directionX = delta.x < 0 ? kDirections[4] : kDirections[5];
    const Direction& directionY = delta.y < 0 ? kDirections[0] : kDirections[1];
    const Direction& directionZ = delta.z < 0 ? kDirections[2] : kDirections[3];
    if (delta.x == 0) {
        return isUnobstructed(region, from, directionY) || isUnobstructed(region, from, directionZ);
    }
    if (delta.y == 0) {
        return isUnobstructed(region, from, directionX) || isUnobstructed(region, from, directionZ);
    }
    return isUnobstructed(region, from, directionX) || isUnobstructed(region, from, directionY);
}

[[nodiscard]] bool hasSubstrateAccess(const SculkRegion& region, const DecorationPos& position,
                                      const std::int32_t stateId) {
    return std::ranges::any_of(kDirections, [&](const Direction& direction) {
        return SculkRegion::hasFace(stateId, direction) &&
               sculk::replaceable().contains(region.blockPath(offsetBy(position, direction)));
    });
}

[[nodiscard]] std::optional<DecorationPos>
findMovementPosition(const SculkRegion& region, const DecorationPos& position, random::RandomSource& random) {
    std::optional<DecorationPos> selected;
    std::vector<DecorationPos> offsets = nonCornerNeighbours();
    sculk::shuffle(offsets, random);
    for (const DecorationPos& delta : offsets) {
        const DecorationPos neighbour = offsetBy(position, delta);
        const std::optional<std::int32_t> neighbourStateId = region.get(neighbour);
        if (!neighbourStateId.has_value() || SculkRegion::behaviour(*neighbourStateId) == SculkBehaviour::Default) {
            continue;
        }
        if (!isMovementUnobstructed(region, position, neighbour, delta)) {
            continue;
        }
        selected = neighbour;
        if (SculkRegion::isVein(*neighbourStateId) && hasSubstrateAccess(region, neighbour, *neighbourStateId)) {
            break;
        }
    }
    return selected;
}

/// parity: `SculkSpreader.ChargeCursor`, the ten walkers a patch grows from.
class ChargeCursor {
public:
    ChargeCursor(const DecorationPos& position, std::int32_t charge) : position_(position), charge_(charge) {}

    void update(SculkRegion& region, const DecorationPos& origin, random::RandomSource& random);

private:
    [[nodiscard]] std::int32_t useChargeFromSculk(SculkRegion& region, const DecorationPos& origin,
                                                  random::RandomSource& random);

    [[nodiscard]] std::int32_t attemptUseCharge(SculkRegion& region, const DecorationPos& origin,
                                                random::RandomSource& random, SculkBehaviour behaviour);

    DecorationPos position_;
    std::int32_t charge_;
    std::int32_t decayDelay_ = 1;
    std::int32_t updateDelay_ = 0;
    std::optional<std::vector<const Direction*>> facings_;
};

std::int32_t ChargeCursor::useChargeFromSculk(SculkRegion& region, const DecorationPos& origin,
                                              random::RandomSource& random) {
    if (charge_ == 0 || random.nextInt(5) != 0) {
        return charge_;
    }
    const bool isCloseToOrigin = position_ == origin;
    if (!isCloseToOrigin && canPlaceGrowth(region, position_)) {
        if (random.nextInt(50) < charge_) {
            const DecorationPos growthPosition = offsetBy(position_, kDirections[1]);
            const std::optional<std::int32_t> above = region.get(growthPosition);
            const bool waterlogged = above.has_value() && registry::blockFluids::hasFluid(*above);
            std::int32_t stateId = 0;
            if (random.nextInt(11) == 0) {
                stateId = waterlogged ? sculk::states().shriekerWaterlogged : sculk::states().shrieker;
            } else {
                stateId = waterlogged ? sculk::states().sensorWaterlogged : sculk::states().sensor;
            }
            region.set(growthPosition, stateId);
        }
        return std::max(0, charge_ - 50);
    }

    if (random.nextInt(10) != 0) {
        return charge_;
    }
    return charge_ - (isCloseToOrigin ? 1 : decayPenalty(position_, origin, charge_));
}

std::int32_t ChargeCursor::attemptUseCharge(SculkRegion& region, const DecorationPos& origin,
                                            random::RandomSource& random, const SculkBehaviour behaviour) {
    switch (behaviour) {
    case SculkBehaviour::Default:
        return decayDelay_ > 0 ? charge_ : 0;
    case SculkBehaviour::Vein:
        if (attemptPlaceSculk(region, position_, random)) {
            return charge_ - 1;
        }
        if (random.nextInt(5) == 0) {
            return wire::common::toIntLikeJava(static_cast<double>(static_cast<float>(charge_) * 0.5F));
        }
        return charge_;
    case SculkBehaviour::Sculk:
    default:
        return useChargeFromSculk(region, origin, random);
    }
}

void ChargeCursor::update(SculkRegion& region, const DecorationPos& origin, random::RandomSource& random) {
    if (charge_ <= 0) {
        return;
    }
    if (updateDelay_ > 0) {
        --updateDelay_;
        return;
    }

    std::int32_t currentStateId = region.get(position_).value_or(registry::blocks::air().stateId);
    SculkBehaviour behaviour = SculkRegion::behaviour(currentStateId);
    if (attemptSpreadVein(region, position_, currentStateId, facings_, behaviour) &&
        behaviour != SculkBehaviour::Sculk) {
        currentStateId = region.get(position_).value_or(registry::blocks::air().stateId);
        behaviour = SculkRegion::behaviour(currentStateId);
    }

    charge_ = attemptUseCharge(region, origin, random, behaviour);
    if (charge_ <= 0) {
        if (behaviour == SculkBehaviour::Vein) {
            sculk::dischargeVein(region, position_, currentStateId);
        }
        return;
    }

    const std::optional<DecorationPos> transferPosition = findMovementPosition(region, position_, random);
    if (transferPosition.has_value()) {
        if (behaviour == SculkBehaviour::Vein) {
            sculk::dischargeVein(region, position_, currentStateId);
        }
        position_ = *transferPosition;
        const std::int32_t dx = position_.x - origin.x;
        const std::int32_t dz = position_.z - origin.z;
        if (dx * dx + dz * dz >= 225) {
            charge_ = 0;
            return;
        }
        currentStateId = region.get(position_).value_or(registry::blocks::air().stateId);
    }

    if (SculkRegion::behaviour(currentStateId) != SculkBehaviour::Default) {
        std::vector<const Direction*> faces;
        for (const Direction& direction : kDirections) {
            if (SculkRegion::hasFace(currentStateId, direction)) {
                faces.push_back(&direction);
            }
        }
        facings_ = std::move(faces);
    }
    decayDelay_ = behaviour == SculkBehaviour::Default ? std::max(decayDelay_ - 1, 0) : 1;
    updateDelay_ = 1;
}

bool growSculkPatch(SculkRegion& region, random::RandomSource& random, const DecorationPos& origin) {
    if (!region.canSpreadFrom(origin)) {
        return false;
    }
    std::vector<ChargeCursor> cursors(10, ChargeCursor{origin, 32});
    for (std::int32_t round = 0; round < 64; ++round) {
        for (ChargeCursor& cursor : cursors) {
            cursor.update(region, origin, random);
        }
    }
    return true;
}

[[nodiscard]] bool hasSturdyFloor(const SculkRegion& region, const DecorationPos& position) {
    return region.hasSturdyFace(offsetBy(position, kDirections[0]), kDirections[1]);
}

/// parity: 26.3 catalyst step, `random_chance` 0.5 strictly below.
void placeCatalyst(SculkRegion& region, random::RandomSource& random, const DecorationPos& origin) {
    if (random.nextFloat() < 0.5F && hasSturdyFloor(region, origin)) {
        region.set(origin, sculk::states().catalyst);
    }
}

/// parity: ancient city shrieker step, one to three tries within two blocks, wanting air on a sturdy floor.
void placeShriekers(SculkRegion& region, random::RandomSource& random, const DecorationPos& origin) {
    const std::int32_t count = random.nextInt(3) + 1;
    for (std::int32_t index = 0; index < count; ++index) {
        const std::int32_t dx = random.nextInt(5) - 2;
        const std::int32_t dy = random.nextInt(5) - 2;
        const std::int32_t dz = random.nextInt(5) - 2;
        const DecorationPos target{origin.x + dx, origin.y + dy, origin.z + dz};
        const std::optional<std::int32_t> here = region.get(target);
        if (!here.has_value() || !registry::blocks::isAir(*here) || !hasSturdyFloor(region, target)) {
            continue;
        }
        static const std::int32_t summoning = [] {
            const std::optional<std::int32_t> state =
                registry::blocks::stateIdOf(wire::common::Identifier::vanilla("sculk_shrieker"),
                                            {{"can_summon", "true"}, {"shrieking", "false"}, {"waterlogged", "false"}});
            return state.value_or(sculk::states().shrieker);
        }();
        region.set(target, summoning);
    }
}

/// parity: `sculk_patch_deep_dark`, a `sequence` of the patch and then the catalyst.
bool placeSculkPatchAt(SculkRegion& region, random::RandomSource& random, const DecorationPos& origin) {
    if (!growSculkPatch(region, random, origin)) {
        return false;
    }
    placeCatalyst(region, random, origin);
    return true;
}

/// parity: `sculk_patch_ancient_city`.
bool placeAncientCitySculkPatchAt(SculkRegion& region, random::RandomSource& random, const DecorationPos& origin) {
    if (!growSculkPatch(region, random, origin)) {
        return false;
    }
    placeCatalyst(region, random, origin);
    placeShriekers(region, random, origin);
    return true;
}

void placeSculkPatches(SculkRegion& region, random::RandomSource& random,
                       const surface::SurfaceRuleEnvironment& environment) {
    const std::int32_t minY = region.chunk().verticalBounds().minY();
    for (std::int32_t attempt = 0; attempt < 256; ++attempt) {
        const std::int32_t x = region.chunk().chunkX() * 16 + random.nextInt(16);
        const std::int32_t z = region.chunk().chunkZ() * 16 + random.nextInt(16);
        const DecorationPos origin{.x = x, .y = minY + random.nextInt(321), .z = z};
        if (biomePath(environment.biomeAt(x, origin.y, z)) != "deep_dark") {
            continue;
        }
        static_cast<void>(placeSculkPatchAt(region, random, origin));
    }
}

void placeSculkVeins(SculkRegion& region, random::RandomSource& random,
                     const surface::SurfaceRuleEnvironment& environment) {
    const std::int32_t minY = region.chunk().verticalBounds().minY();
    const std::int32_t count = 204 + random.nextInt(47);
    for (std::int32_t attempt = 0; attempt < count; ++attempt) {
        const std::int32_t x = region.chunk().chunkX() * 16 + random.nextInt(16);
        const std::int32_t z = region.chunk().chunkZ() * 16 + random.nextInt(16);
        const DecorationPos origin{.x = x, .y = minY + random.nextInt(321), .z = z};
        if (biomePath(environment.biomeAt(x, origin.y, z)) != "deep_dark") {
            continue;
        }
        static_cast<void>(sculk::placeMultifaceGrowth(region, random, origin));
    }
}

} // namespace

const std::set<std::string, std::less<>>& sculkDecoration::placedIds() {
    static const std::set<std::string, std::less<>> ids = {"sculk_patch_deep_dark", "sculk_patch_ancient_city",
                                                           "sculk_vein"};
    return ids;
}

void sculkDecoration::decorateFeature(Chunk& chunk, const std::int64_t seed,
                                      const surface::SurfaceRuleEnvironment& environment,
                                      const std::int32_t globalIndex, ExternalStateReader externalBlockState) {
    SculkRegion region{chunk, std::move(externalBlockState)};
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    random::setFeatureSeed(random, decorationSeed, globalIndex, kUndergroundDecorationStep);
    if (globalIndex == kSculkVeinIndex) {
        placeSculkVeins(region, random, environment);
    } else if (globalIndex == kSculkPatchIndex) {
        placeSculkPatches(region, random, environment);
    }
}

void sculkDecoration::decorate(Chunk& chunk, const std::int64_t seed,
                               const surface::SurfaceRuleEnvironment& environment,
                               ExternalStateReader externalBlockState) {
    SculkRegion region{chunk, std::move(externalBlockState)};
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    random::setFeatureSeed(random, decorationSeed, kSculkVeinIndex, kUndergroundDecorationStep);
    placeSculkVeins(region, random, environment);
    random::setFeatureSeed(random, decorationSeed, kSculkPatchIndex, kUndergroundDecorationStep);
    placeSculkPatches(region, random, environment);
}

bool sculkDecoration::placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin,
                              const std::string_view placedFeature) {
    SculkRegion region{chunk, nullptr};
    if (placedFeature == "sculk_patch_deep_dark") {
        return placeSculkPatchAt(region, random, origin);
    }
    if (placedFeature == "sculk_patch_ancient_city") {
        return placeAncientCitySculkPatchAt(region, random, origin);
    }
    if (placedFeature == "sculk_vein") {
        return sculk::placeMultifaceGrowth(region, random, origin);
    }
    return false;
}

} // namespace basalt::server::world::terrain::decoration
