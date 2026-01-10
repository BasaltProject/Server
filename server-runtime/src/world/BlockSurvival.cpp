#include <basalt/server/registry/BlockConnections.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockSupport.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/BlockSurvival.hpp>
#include <basalt/server/runtime/world/WorldLight.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <string>

namespace basalt::server::runtime::world::blockSurvival {
namespace {

using registry::BlockFace;

/// parity: `SpeleothemBlock`'s two blocks in 26.2.
constexpr std::array<std::string_view, 2> kSpeleothems{"pointed_dripstone", "sulfur_spike"};

/// The crops whose `canSurvive` is `CropBlock`'s: soil underneath *and* light on top.
constexpr std::array<std::string_view, 6> kLightHungryCrops{"wheat",     "carrots",          "potatoes",
                                                            "beetroots", "torchflower_crop", "pitcher_crop"};

[[nodiscard]] std::int32_t stateAt(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    return world.blockState(x, y, z).value_or(registry::blocks::air().stateId);
}

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : block->path();
}

[[nodiscard]] std::string valueOf(std::int32_t stateId, const std::string& key) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(key);
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] bool isFullSnow(std::int32_t stateId) {
    return pathOf(stateId) == "snow" && valueOf(stateId, "layers") == "8";
}

/// parity: `PitcherCropBlock.canSurvive`, where only the lower half asks about the light.
[[nodiscard]] bool isLowerHalf(std::int32_t stateId) {
    const std::string half = valueOf(stateId, "half");
    return half.empty() || half == "lower";
}

/// parity: `SpeleothemBlock.isValidSpeleothemPlacement`, which hangs a spike off a sturdy face *or*
/// off another spike pointing the same way. The probe that built the support table tries each block
/// in its default state, and the default spike points up, so a hanging column looked to it like a
/// spike held up by nothing.
[[nodiscard]] bool speleothemSurvives(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                      std::int32_t stateId, const std::string& path) {
    const bool down = valueOf(stateId, "vertical_direction") != "up";
    const std::int32_t behind = stateAt(world, x, down ? y + 1 : y - 1, z);
    if (registry::blockShapes::isFaceSturdy(behind, down ? BlockFace::Down : BlockFace::Up)) {
        return true;
    }
    if (pathOf(behind) != path) {
        return false;
    }
    return (valueOf(behind, "vertical_direction") != "up") == down;
}

/// parity: `CactusBlock.canSurvive`, written out here because it is one of the rules a probe of one
/// neighbour at a time cannot express: it asks what must *not* be beside it.
[[nodiscard]] bool cactusSurvives(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    for (const std::int32_t beside : {stateAt(world, x, y, z - 1), stateAt(world, x, y, z + 1),
                                      stateAt(world, x - 1, y, z), stateAt(world, x + 1, y, z)}) {
        if (registry::blockShapes::isSolidState(beside)) {
            return false;
        }
    }
    const std::int32_t below = stateAt(world, x, y - 1, z);
    const bool supported = pathOf(below) == "cactus" || registry::blockConnections::supportsCactus(below);
    return supported && !registry::blockSupport::holdsWater(stateAt(world, x, y + 1, z));
}

/// Whether there is water at this position, beside it, or beside the block under it.
///
/// Sugar cane asks about the last of those and kelp about the first, and neither can be answered
/// from the block alone. Asking generously is deliberate: it can only keep a block standing.
[[nodiscard]] bool waterAbout(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    // The block's own square: today no block that needs support and cares about water is itself a
    // water-holding state, so this only ever says yes where the neighbours would too.
    if (registry::blockSupport::holdsWater(stateAt(world, x, y, z))) {
        return true;
    }
    for (const std::int32_t level : {y, y - 1}) {
        if (registry::blockSupport::holdsWater(stateAt(world, x - 1, level, z)) ||
            registry::blockSupport::holdsWater(stateAt(world, x + 1, level, z)) ||
            registry::blockSupport::holdsWater(stateAt(world, x, level, z - 1)) ||
            registry::blockSupport::holdsWater(stateAt(world, x, level, z + 1))) {
            return true;
        }
    }
    return false;
}

} // namespace

bool isSupported(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::optional<std::int32_t> stateId,
                 const ScaffoldingHasFallen& scaffoldingHasFallen, bool hasSkylight) {
    const std::int32_t state = stateId.value_or(stateAt(world, x, y, z));
    const std::string path = pathOf(state);

    if (path == "cactus") {
        return cactusSurvives(world, x, y, z);
    }
    if (path == "scaffolding") {
        return !scaffoldingHasFallen || !scaffoldingHasFallen(world, x, y, z);
    }
    if (std::ranges::find(kSpeleothems, path) != kSpeleothems.end()) {
        return speleothemSurvives(world, x, y, z, state, path);
    }
    // parity: `SnowLayerBlock.canSurvive`'s last clause, snow standing on a full stack of snow. The
    // probe tries each block in its default state, and a default snow layer is one deep.
    if (path == "snow" && isFullSnow(stateAt(world, x, y - 1, z))) {
        return true;
    }
    // parity: `CropBlock.canSurvive`, which asks for light eight on top of its soil. The probe
    // answers one neighbour at a time and cannot see the sky, so it was told the light is fifteen
    // and the light half is checked here instead.
    if (std::ranges::find(kLightHungryCrops, path) != kLightHungryCrops.end() && isLowerHalf(state)) {
        if (worldLight::rawBrightness(world, x, y, z, hasSkylight) < kCropSurvivalLight) {
            return false;
        }
    }
    // Nothing the table skips is refused by it either, so this is a shortcut rather than a rule of
    // its own: it is here because "the table does not model this" and "this stands anywhere" are
    // different reasons to say yes.
    if (!registry::blockSupport::needsSupport(state) || !registry::blockSupport::isModelled(state)) {
        return true;
    }
    return registry::blockSupport::canSurvive(state, stateAt(world, x, y - 1, z), stateAt(world, x, y + 1, z),
                                              stateAt(world, x, y, z - 1), stateAt(world, x, y, z + 1),
                                              stateAt(world, x - 1, y, z), stateAt(world, x + 1, y, z),
                                              waterAbout(world, x, y, z));
}

} // namespace basalt::server::runtime::world::blockSurvival
