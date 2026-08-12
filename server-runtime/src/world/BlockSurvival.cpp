#include <basalt/server/registry/BlockConnections.hpp>
#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockSupport.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/config/GameplayRules.hpp>
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

constexpr std::array<std::string_view, 6> kLightHungryCrops{"wheat",     "carrots",          "potatoes",
                                                            "beetroots", "torchflower_crop", "pitcher_crop"};

[[nodiscard]] std::int32_t stateAt(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    return world.blockState(x, y, z).value_or(registry::blocks::air().stateId);
}

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : block->path();
}

/// parity: `ChorusFlowerBlock.canSurvive` word for word.
[[nodiscard]] bool chorusFlowerSurvives(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::int32_t below = stateAt(world, x, y - 1, z);
    const std::string belowPath = pathOf(below);
    // parity: `#minecraft:supports_chorus_flower`, which is end stone and little else.
    if (belowPath == "chorus_plant" || belowPath == "end_stone") {
        return true;
    }
    if (below != registry::blocks::air().stateId) {
        return false;
    }
    bool oneNeighbour = false;
    for (const BlockFace face : {BlockFace::North, BlockFace::South, BlockFace::West, BlockFace::East}) {
        const std::string side =
            pathOf(stateAt(world, x + registry::stepX(face), y + registry::stepY(face), z + registry::stepZ(face)));
        if (side == "chorus_plant") {
            if (oneNeighbour) {
                return false;
            }
            oneNeighbour = true;
        } else if (side != "air") {
            return false;
        }
    }
    return oneNeighbour;
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

/// parity: `SpeleothemBlock.isValidSpeleothemPlacement`.
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

/// parity: `CactusBlock.canSurvive`.
[[nodiscard]] bool cactusSurvives(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    for (const std::int32_t beside : {stateAt(world, x, y, z - 1), stateAt(world, x, y, z + 1),
                                      stateAt(world, x - 1, y, z), stateAt(world, x + 1, y, z)}) {
        if (world.gameplay().blocks.cactusBreaksFromSolidNeighbors && registry::blockShapes::isSolidState(beside)) {
            return false;
        }
    }
    const std::int32_t below = stateAt(world, x, y - 1, z);
    const bool supported = pathOf(below) == "cactus" || registry::blockConnections::supportsCactus(below);
    return supported && !registry::blockSupport::holdsWater(stateAt(world, x, y + 1, z));
}

[[nodiscard]] bool waterAbout(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    // parity: the jar asks `getFluidState(...).is(FluidTags.WATER)`.
    if (registry::blockFluids::hasWaterFluid(stateAt(world, x, y, z))) {
        return true;
    }
    for (const std::int32_t level : {y, y - 1}) {
        if (registry::blockFluids::hasWaterFluid(stateAt(world, x - 1, level, z)) ||
            registry::blockFluids::hasWaterFluid(stateAt(world, x + 1, level, z)) ||
            registry::blockFluids::hasWaterFluid(stateAt(world, x, level, z - 1)) ||
            registry::blockFluids::hasWaterFluid(stateAt(world, x, level, z + 1))) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] std::optional<BlockFace> faceNamed(std::string_view name) {
    static constexpr std::array<std::pair<std::string_view, BlockFace>, 6> kNames{{{"down", BlockFace::Down},
                                                                                   {"up", BlockFace::Up},
                                                                                   {"north", BlockFace::North},
                                                                                   {"south", BlockFace::South},
                                                                                   {"west", BlockFace::West},
                                                                                   {"east", BlockFace::East}}};
    const auto found = std::ranges::find_if(kNames, [name](const auto& one) { return one.first == name; });
    return found == kNames.end() ? std::nullopt : std::optional{found->second};
}

/// an empty set means none can, which is the bell hung between two walls. parity: only the blocks
[[nodiscard]] std::optional<std::uint8_t> hangsOn(std::int32_t stateId) {
    const auto only = [](BlockFace face) { return static_cast<std::uint8_t>(1U << static_cast<std::uint8_t>(face)); };
    const std::string path = pathOf(stateId);
    // parity: `TripWireHookBlock`.
    static constexpr std::array<std::string_view, 8> kBehindTheFace{"tripwire_hook",     "wall_torch",  "wall_sign",
                                                                    "wall_hanging_sign", "wall_banner", "wall_fan",
                                                                    "amethyst_cluster",  "amethyst_bud"};
    if (path == "ladder" ||
        std::ranges::any_of(kBehindTheFace, [&path](std::string_view suffix) { return path.ends_with(suffix); })) {
        const std::optional<BlockFace> facing = faceNamed(valueOf(stateId, "facing"));
        return facing.has_value() ? std::optional{only(registry::opposite(*facing))} : std::nullopt;
    }
    // parity: `FaceAttachedHorizontalDirectionalBlock.getConnectedDirection`.
    if (path == "lever" || path == "grindstone" || path.ends_with("_button")) {
        const std::string face = valueOf(stateId, "face");
        if (face == "floor") {
            return only(BlockFace::Down);
        }
        if (face == "ceiling") {
            return only(BlockFace::Up);
        }
        const std::optional<BlockFace> facing = faceNamed(valueOf(stateId, "facing"));
        return facing.has_value() ? std::optional{only(registry::opposite(*facing))} : std::nullopt;
    }
    // parity: `LanternBlock.getConnectedDirection`.
    if (path.ends_with("lantern")) {
        const std::string hanging = valueOf(stateId, "hanging");
        if (!hanging.empty()) {
            return only(hanging == "true" ? BlockFace::Up : BlockFace::Down);
        }
        return std::nullopt;
    }
    // parity: `SpeleothemBlock.updateShape` leaves early for anything that is not up or down.
    if (std::ranges::find(kSpeleothems, path) != kSpeleothems.end()) {
        return only(valueOf(stateId, "vertical_direction") != "up" ? BlockFace::Up : BlockFace::Down);
    }
    // parity: `CoralPlantBlock`.
    if (path.ends_with("_coral") || path.ends_with("_coral_fan")) {
        return only(BlockFace::Down);
    }
    // parity: `ChorusFlowerBlock.updateShape` is `directionToNeighbour != Direction.UP`.
    if (path == "chorus_flower") {
        return static_cast<std::uint8_t>(only(BlockFace::Down) | only(BlockFace::North) | only(BlockFace::South) |
                                         only(BlockFace::West) | only(BlockFace::East));
    }
    // parity: `BigDripleafStemBlock`, the one that asks about both ends.
    if (path == "big_dripleaf_stem") {
        return static_cast<std::uint8_t>(only(BlockFace::Down) | only(BlockFace::Up));
    }
    // parity: `CocoaBlock`, which hangs on the jungle log its own face points at.
    if (path == "cocoa") {
        const std::optional<BlockFace> facing = faceNamed(valueOf(stateId, "facing"));
        return facing.has_value() ? std::optional{only(*facing)} : std::nullopt;
    }
    // parity: `CandleBlock` has a `canSurvive` and no `updateShape` that answers air.
    if (path.ends_with("candle") && !path.ends_with("candle_cake")) {
        return std::uint8_t{0};
    }
    // parity: `BellBlock`.
    if (path == "bell") {
        const std::string attachment = valueOf(stateId, "attachment");
        if (attachment == "double_wall") {
            return std::uint8_t{0};
        }
        if (attachment == "floor") {
            return only(BlockFace::Down);
        }
        if (attachment == "ceiling") {
            return only(BlockFace::Up);
        }
        const std::optional<BlockFace> facing = faceNamed(valueOf(stateId, "facing"));
        return facing.has_value() ? std::optional{only(*facing)} : std::nullopt;
    }
    // parity: `DoorBlock.updateShape`.
    if (path.ends_with("_door") && !path.ends_with("_trapdoor")) {
        return valueOf(stateId, "half") == "lower" ? std::optional{only(BlockFace::Down)}
                                                   : std::optional{std::uint8_t{0}};
    }
    // parity: `CeilingHangingSignBlock`, `SporeBlossomBlock`, `HangingRootsBlock`.
    const bool ceilingSign = path.ends_with("_hanging_sign") && !path.ends_with("_wall_hanging_sign");
    if (ceilingSign || path == "spore_blossom" || path == "hanging_roots" || path == "mangrove_propagule" ||
        path == "dirt_path" || path == "farmland") {
        return only(BlockFace::Up);
    }
    // parity: `RepeaterBlock`.
    if (path == "repeater" || path == "comparator" || path == "redstone_wire") {
        return only(BlockFace::Down);
    }
    // parity: `VineBlock.updateShape` is the one written inside out.
    if (path == "vine") {
        return static_cast<std::uint8_t>(only(BlockFace::Up) | only(BlockFace::North) | only(BlockFace::South) |
                                         only(BlockFace::West) | only(BlockFace::East));
    }
    // parity: `BaseTorchBlock`, `CakeBlock`, `CandleCakeBlock`, `BasePressurePlateBlock`.
    const bool standingSign =
        path.ends_with("_sign") && !path.ends_with("_wall_sign") && !path.ends_with("_hanging_sign");
    const bool standingBanner = path.ends_with("_banner") && !path.ends_with("_wall_banner");
    if (standingSign || standingBanner || path == "torch" || path == "soul_torch" || path == "redstone_torch" ||
        path == "cake" || path.ends_with("candle_cake") || path.ends_with("_pressure_plate") || path == "flower_pot" ||
        path.starts_with("potted_") || path == "big_dripleaf") {
        return only(BlockFace::Down);
    }
    return std::nullopt;
}

} // namespace

bool loosenedBy(std::int32_t stateId, std::optional<registry::BlockFace> towards) {
    if (!towards.has_value()) {
        return true;
    }
    const std::optional<std::uint8_t> sides = hangsOn(stateId);
    return !sides.has_value() || (*sides & (1U << static_cast<std::uint8_t>(*towards))) != 0;
}

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
    // parity: `ChorusFlowerBlock.canSurvive`.
    if (path == "chorus_flower") {
        return chorusFlowerSurvives(world, x, y, z);
    }
    // parity: `SnowLayerBlock.canSurvive`'s last clause.
    if (path == "snow" && isFullSnow(stateAt(world, x, y - 1, z))) {
        return true;
    }
    // parity: `CropBlock.canSurvive`.
    if (std::ranges::find(kLightHungryCrops, path) != kLightHungryCrops.end() && isLowerHalf(state)) {
        if (worldLight::rawBrightness(world, x, y, z, hasSkylight) < kCropSurvivalLight) {
            return false;
        }
    }
    // parity: `BaseRailBlock.shouldBeRemoved`.
    if (path == "rail" || path == "powered_rail" || path == "detector_rail" || path == "activator_rail") {
        if (!registry::blockShapes::supportsRigid(stateAt(world, x, y - 1, z), registry::BlockFace::Up)) {
            return false;
        }
        const std::string shape = valueOf(state, "shape");
        std::optional<registry::BlockFace> climbing;
        if (shape == "ascending_east") {
            climbing = registry::BlockFace::East;
        } else if (shape == "ascending_west") {
            climbing = registry::BlockFace::West;
        } else if (shape == "ascending_north") {
            climbing = registry::BlockFace::North;
        } else if (shape == "ascending_south") {
            climbing = registry::BlockFace::South;
        }
        if (!climbing.has_value()) {
            return true;
        }
        return registry::blockShapes::supportsRigid(
            stateAt(world, x + registry::stepX(*climbing), y, z + registry::stepZ(*climbing)), registry::BlockFace::Up);
    }
    // parity: `RedStoneWireBlock.canSurviveOn` is `isFaceSturdy(UP) || is(HOPPER)`.
    if (path == "redstone_wire") {
        const std::int32_t below = stateAt(world, x, y - 1, z);
        const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(below);
        return registry::blockShapes::isFaceSturdy(below, registry::BlockFace::Up) ||
               (block != nullptr && block->path() == "hopper");
    }
    // parity: `BaseTorchBlock.canSurvive` is `canSupportCenter(below, UP)` and.
    if (path == "torch" || path == "soul_torch" || path == "redstone_torch") {
        return registry::blockShapes::supportsCentre(stateAt(world, x, y - 1, z), registry::BlockFace::Up);
    }
    if (path == "wall_torch" || path == "soul_wall_torch" || path == "redstone_wall_torch") {
        const std::optional<registry::BlockFace> facing =
            faceNamed(registry::blockStateDefinitions::propertiesOf(state).contains("facing")
                          ? registry::blockStateDefinitions::propertiesOf(state).at("facing")
                          : std::string{});
        if (!facing.has_value()) {
            return true;
        }
        const registry::BlockFace behind = registry::opposite(*facing);
        return registry::blockShapes::isFaceSturdy(
            stateAt(world, x + registry::stepX(behind), y + registry::stepY(behind), z + registry::stepZ(behind)),
            *facing);
    }
    // parity: `DoorBlock.canSurvive`.
    if (path.ends_with("_door") && !path.ends_with("_trapdoor")) {
        const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(state);
        const auto half = properties.find("half");
        if (half != properties.end()) {
            const std::int32_t below = stateAt(world, x, y - 1, z);
            if (half->second == "lower") {
                return registry::blockShapes::isFaceSturdy(below, registry::BlockFace::Up);
            }
            const wire::common::Identifier* under = registry::blockStateDefinitions::blockOf(below);
            const wire::common::Identifier* mine = registry::blockStateDefinitions::blockOf(state);
            return under != nullptr && mine != nullptr && *under == *mine;
        }
    }
    // parity: `DoublePlantBlock.canSurvive`.
    if (const std::string half = valueOf(state, "half"); half == "upper" || half == "lower") {
        const std::int32_t below = stateAt(world, x, y - 1, z);
        const wire::common::Identifier* mine = registry::blockStateDefinitions::blockOf(state);
        if (half == "upper") {
            const wire::common::Identifier* under = registry::blockStateDefinitions::blockOf(below);
            return under != nullptr && mine != nullptr && *under == *mine && valueOf(below, "half") == "lower";
        }
        if (mine != nullptr) {
            if (const std::optional<std::int32_t> partner =
                    registry::blockStateDefinitions::stateIdOf(*mine, {{"half", "upper"}}, state);
                partner.has_value()) {
                return registry::blockSupport::canSurvive(state, below, *partner, stateAt(world, x, y, z - 1),
                                                          stateAt(world, x, y, z + 1), stateAt(world, x - 1, y, z),
                                                          stateAt(world, x + 1, y, z), waterAbout(world, x, y, z));
            }
        }
    }
    // parity: `BasePressurePlateBlock.canSurvive` is `canSupportRigidBlock(below) ||.
    if (path.ends_with("_pressure_plate")) {
        const std::int32_t below = stateAt(world, x, y - 1, z);
        return registry::blockShapes::supportsRigid(below, registry::BlockFace::Up) ||
               registry::blockShapes::supportsCentre(below, registry::BlockFace::Up);
    }
    // parity: `HangingMossBlock.canStayAtPosition` is `MultifaceBlock.canAttachTo(UP)` or more moss.
    if (path == "pale_hanging_moss") {
        const std::int32_t above = stateAt(world, x, y + 1, z);
        return registry::blockShapes::canAttachTo(above, registry::BlockFace::Down) ||
               pathOf(above) == "pale_hanging_moss";
    }
    if (!registry::blockSupport::needsSupport(state) || !registry::blockSupport::isModelled(state)) {
        return true;
    }
    return registry::blockSupport::canSurvive(state, stateAt(world, x, y - 1, z), stateAt(world, x, y + 1, z),
                                              stateAt(world, x, y, z - 1), stateAt(world, x, y, z + 1),
                                              stateAt(world, x - 1, y, z), stateAt(world, x + 1, y, z),
                                              waterAbout(world, x, y, z));
}

} // namespace basalt::server::runtime::world::blockSurvival
