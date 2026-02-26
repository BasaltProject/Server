#include <basalt/server/registry/BlockConnections.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockSupport.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/ScheduledBlocks.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/server/runtime/world/WorldLight.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <map>
#include <string>

namespace basalt::server::runtime::world::scheduledBlocks {
namespace {

/// parity: `ScaffoldingBlock`'s own reach, and `FrostedIceBlock`'s numbers.
constexpr std::int32_t kScaffoldReach = 7;

/// parity: `BaseCoralPlantTypeBlock.tryScheduleDieTick`, `60 + random.nextInt(40)`.
constexpr std::int32_t kCoralGraceTicks = 60;
constexpr std::int32_t kCoralGraceSpread = 40;

/// parity: `LiquidBlock.BUBBLE_COLUMN_CHECK_DELAY` and `BubbleColumnBlock.updateShape`'s own five.
constexpr std::int32_t kColumnCheckDelay = 20;
constexpr std::int32_t kColumnStepDelay = 5;

/// parity: `SpeleothemBlock.updateShape`'s two delays.
constexpr std::int32_t kStalactiteDelay = 2;
constexpr std::int32_t kStalagmiteDelay = 1;
constexpr std::int32_t kFrostedMaxAge = 3;
constexpr std::int32_t kFrostedNeighbours = 4;

/// parity: `FrostedIceBlock.neighborChanged` asks for `fewerNeigboursThan(level, pos, 2)`.
constexpr std::int32_t kFrostedMeltNeighbours = 2;

/// A trail of frost walker ice is at most a few hundred squares; this only stops a runaway.
constexpr std::int32_t kFrostedChainLimit = 512;
/// parity: `Mth.nextInt(random, 20, 40)`, inclusivo por los dos lados, que es el que el hielo
/// fragil se reserva a si mismo y a cada vecino que sobrevive.
constexpr std::int32_t kFrostedRetryLow = 20;
constexpr std::int32_t kFrostedRetryHigh = 40;

/// parity: `FrostedIceBlock.onPlace`'s own `Mth.nextInt(random, 60, 120)`, the wait before a fresh
/// piece of frosted ice first looks at whether it should melt.
constexpr std::int32_t kFrostedFirstLow = 60;
constexpr std::int32_t kFrostedFirstHigh = 120;
constexpr std::int32_t kMeltLight = 11;
constexpr std::int32_t kDriedGhastMax = 3;
constexpr std::int32_t kDriedGhastDelay = 5000;

/// parity: `SnifferEggBlock`'s own three numbers.
constexpr std::int32_t kSnifferMaxHatch = 2;
constexpr std::int32_t kRegularHatchTicks = 24000;
constexpr std::int32_t kBoostedHatchTicks = 12000;
constexpr std::int32_t kHatchSpreadTicks = 300;

/// parity: `FrogspawnBlock`'s own two delays.
constexpr std::int32_t kFrogspawnMinDelay = 3600;
constexpr std::int32_t kFrogspawnMaxDelay = 12000;

constexpr std::array<registry::BlockFace, 6> kAllFaces{registry::BlockFace::Down,  registry::BlockFace::Up,
                                                       registry::BlockFace::North, registry::BlockFace::South,
                                                       registry::BlockFace::West,  registry::BlockFace::East};

constexpr std::array<registry::BlockFace, 4> kHorizontal{registry::BlockFace::North, registry::BlockFace::South,
                                                         registry::BlockFace::West, registry::BlockFace::East};

[[nodiscard]] std::int32_t offsetX(registry::BlockFace face) {
    if (face == registry::BlockFace::West) {
        return -1;
    }
    return face == registry::BlockFace::East ? 1 : 0;
}

[[nodiscard]] std::int32_t offsetY(registry::BlockFace face) {
    if (face == registry::BlockFace::Down) {
        return -1;
    }
    return face == registry::BlockFace::Up ? 1 : 0;
}

[[nodiscard]] std::int32_t offsetZ(registry::BlockFace face) {
    if (face == registry::BlockFace::North) {
        return -1;
    }
    return face == registry::BlockFace::South ? 1 : 0;
}

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : std::string{block->path()};
}

[[nodiscard]] std::optional<std::int32_t> numberProperty(std::int32_t stateId, const std::string& name) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(name);
    if (found == properties.end()) {
        return std::nullopt;
    }
    std::int32_t value = 0;
    const std::from_chars_result result =
        std::from_chars(found->second.data(), found->second.data() + found->second.size(), value);
    if (result.ec != std::errc{} || result.ptr != found->second.data() + found->second.size()) {
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] std::string textProperty(std::int32_t stateId, const std::string& name) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(name);
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] std::int32_t namedState(const std::string& name) {
    const registry::BlockState* block = registry::blocks::named(name);
    return block == nullptr ? registry::blocks::air().stateId : block->stateId;
}

[[nodiscard]] std::int32_t waterState() {
    static const std::int32_t state = namedState("water");
    return state;
}

/// parity: `BaseCoralPlantTypeBlock.scanForWater`, its own state included.
[[nodiscard]] bool waterAbout(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) {
    if (registry::blockSupport::holdsWater(stateId)) {
        return true;
    }
    for (const registry::BlockFace face : kAllFaces) {
        const std::optional<std::int32_t> neighbour =
            world.blockState(x + offsetX(face), y + offsetY(face), z + offsetZ(face));
        if (neighbour.has_value() && registry::blockSupport::holdsWater(*neighbour)) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool isScaffolding(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    return pathOf(world.blockState(x, y, z).value_or(0)) == "scaffolding";
}

[[nodiscard]] std::int32_t scaffoldDistance(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> below = world.blockState(x, y - 1, z);
    if (!below.has_value()) {
        return kScaffoldReach;
    }
    std::int32_t distance = kScaffoldReach;
    if (isScaffolding(world, x, y - 1, z)) {
        distance = numberProperty(*below, "distance").value_or(kScaffoldReach);
    } else if (registry::blockShapes::isFaceSturdy(*below, registry::BlockFace::Up)) {
        return 0;
    }
    for (const registry::BlockFace face : kHorizontal) {
        const std::int32_t sideX = x + offsetX(face);
        const std::int32_t sideZ = z + offsetZ(face);
        if (!isScaffolding(world, sideX, y, sideZ)) {
            continue;
        }
        const std::optional<std::int32_t> side = world.blockState(sideX, y, sideZ);
        if (!side.has_value()) {
            continue;
        }
        const std::optional<std::int32_t> theirs = numberProperty(*side, "distance");
        if (!theirs.has_value()) {
            continue;
        }
        distance = std::min(distance, *theirs + 1);
        if (distance == 1) {
            break;
        }
    }
    return distance;
}

[[nodiscard]] std::int32_t neighbouringIce(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    std::int32_t count = 0;
    for (const registry::BlockFace face : kAllFaces) {
        const std::optional<std::int32_t> neighbour =
            world.blockState(x + offsetX(face), y + offsetY(face), z + offsetZ(face));
        if (neighbour.has_value() && pathOf(*neighbour) == "frosted_ice") {
            ++count;
        }
    }
    return count;
}

/// One step thinner, or gone. parity: `FrostedIceBlock.slightlyMelt`, which answers **true only when
/// the ice went**: thinning it by one is a `false`, and that answer is what decides whether the caller
/// goes on to the neighbours or just books itself another turn. It books nothing of its own.
bool thin(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t age,
          std::vector<BlockChange>& changes) {
    if (age < kFrostedMaxAge) {
        const std::optional<std::int32_t> thinner = registry::blockStateDefinitions::stateIdOf(
            frostedIceTickType(), {{"age", std::to_string(age + 1)}}, std::nullopt);
        if (thinner.has_value() && world.setBlockState(x, y, z, *thinner)) {
            changes.push_back(BlockChange{.x = x, .y = y, .z = z, .stateId = *thinner});
        }
        return false;
    }
    if (!world.setBlockState(x, y, z, waterState())) {
        return false;
    }
    changes.push_back(BlockChange{.x = x, .y = y, .z = z, .stateId = waterState()});
    return true;
}

/// parity: `BubbleColumnBlock.canOccupy`: water, or a column already.
[[nodiscard]] bool canOccupy(std::int32_t stateId) {
    return stateId == waterState() || pathOf(stateId) == "bubble_column";
}

[[nodiscard]] std::optional<std::int32_t> columnState(bool dragDown) {
    return registry::blockStateDefinitions::stateIdOf(bubbleColumnTickType(), {{"drag", dragDown ? "true" : "false"}},
                                                      std::nullopt);
}

} // namespace

void scheduleFrostedIce(WorldState& world, const std::int32_t x, const std::int32_t y, const std::int32_t z,
                        random::RandomSource& random) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || pathOf(*stateId) != "frosted_ice") {
        return;
    }
    // parity: `onPlace` books without asking whether one is already pending, and `LevelChunkTicks`
    // keeps only one turn per square and type, so the second call changes nothing either way.
    world.scheduleBlockTick(x, y, z, frostedIceTickType(),
                            random.nextIntBetweenInclusive(kFrostedFirstLow, kFrostedFirstHigh));
}

const wire::common::Identifier& frostedIceTickType() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("frosted_ice");
    return instance;
}

const wire::common::Identifier& driedGhastTickType() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("dried_ghast");
    return instance;
}

const wire::common::Identifier& snifferEggTickType() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("sniffer_egg");
    return instance;
}

const wire::common::Identifier& frogspawnTickType() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("frogspawn");
    return instance;
}

const wire::common::Identifier& bubbleColumnTickType() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("bubble_column");
    return instance;
}

const wire::common::Identifier& sugarCaneTickType() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("sugar_cane");
    return instance;
}

/// parity: `FarmlandBlock.canSurvive` over `shouldMaintainFarmland`, and `DirtPathBlock`'s, which is
/// the same question without the crop.
[[nodiscard]] bool soilIsCovered(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                 std::string_view path) {
    const std::optional<std::int32_t> above = world.blockState(x, y + 1, z);
    if (!above.has_value() || !registry::blockShapes::isSolidState(*above)) {
        return false;
    }
    if (path != "farmland") {
        // parity: `DirtPathBlock.canSurvive` lets a fence gate stand on a path, and nothing else.
        return !pathOf(*above).ends_with("fence_gate");
    }
    return !registry::blockConnections::maintainsFarmland(*above);
}

void scheduleCoveredSoil(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return;
    }
    const std::string path = pathOf(*stateId);
    if ((path != "farmland" && path != "dirt_path") || !soilIsCovered(world, x, y, z, path)) {
        return;
    }
    world.scheduleBlockTick(x, y, z, wire::common::Identifier::vanilla(path), 1);
}

std::optional<std::int32_t> coveredSoil(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return std::nullopt;
    }
    const std::string path = pathOf(*stateId);
    if (path != "farmland" && path != "dirt_path") {
        return std::nullopt;
    }
    // parity: `FarmlandBlock.tick` asks again before it gives way; `DirtPathBlock.tick` does not,
    // because nothing else books its turn.
    if (path == "farmland" && !soilIsCovered(world, x, y, z, path)) {
        return std::nullopt;
    }
    const std::int32_t dirt = namedState("dirt");
    if (!world.setBlockState(x, y, z, dirt)) {
        return std::nullopt;
    }
    return dirt;
}

/// Whether this is live coral that is currently out of water.
[[nodiscard]] bool coralIsDrying(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                 std::int32_t stateId, std::string_view path) {
    if (!path.ends_with("coral") && !path.ends_with("coral_block") && !path.ends_with("coral_fan") &&
        !path.ends_with("coral_wall_fan")) {
        return false;
    }
    return !path.starts_with("dead_") && !waterAbout(world, x, y, z, stateId);
}

void scheduleCoralDeath(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int64_t seed) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return;
    }
    const std::string path = pathOf(*stateId);
    if (!coralIsDrying(world, x, y, z, *stateId, path)) {
        return;
    }
    // parity: `BaseCoralPlantTypeBlock.tryScheduleDieTick`, three seconds and a bit, never the same
    // twice: coral out of water keeps its colour for a while before it goes white.
    random::RandomSource random = random::RandomSource::legacy(seed);
    world.scheduleBlockTick(x, y, z, wire::common::Identifier::vanilla(path),
                            kCoralGraceTicks + random.nextInt(kCoralGraceSpread));
}

std::optional<std::int32_t> coral(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return std::nullopt;
    }
    const std::string path = pathOf(*stateId);
    if (!coralIsDrying(world, x, y, z, *stateId, path)) {
        return std::nullopt;
    }
    const std::string dead = "dead_" + path;
    std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    properties.erase("waterlogged");
    std::optional<std::int32_t> deadState =
        registry::blockStateDefinitions::stateIdOf(wire::common::Identifier::vanilla(dead), properties, std::nullopt);
    if (!deadState.has_value()) {
        const registry::BlockState* plain = registry::blocks::named(dead);
        if (plain == nullptr) {
            return std::nullopt;
        }
        deadState = plain->stateId;
    }
    if (!world.setBlockState(x, y, z, *deadState)) {
        return std::nullopt;
    }
    return deadState;
}

std::optional<std::int32_t> scaffolding(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || pathOf(*stateId) != "scaffolding") {
        return std::nullopt;
    }
    const std::int32_t distance = scaffoldDistance(world, x, y, z);
    const bool bottom = distance > 0 && !isScaffolding(world, x, y - 1, z);
    const std::optional<std::int32_t> reshaped = registry::blockStateDefinitions::stateIdOf(
        wire::common::Identifier::vanilla("scaffolding"),
        {{"distance", std::to_string(distance)}, {"bottom", bottom ? "true" : "false"}}, *stateId);
    if (!reshaped.has_value() || *reshaped == *stateId || !world.setBlockState(x, y, z, *reshaped)) {
        return std::nullopt;
    }
    return reshaped;
}

bool scaffoldingHasFallen(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    return scaffoldDistance(world, x, y, z) >= kScaffoldReach;
}

void scheduleScaffolding(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || pathOf(*stateId) != "scaffolding") {
        return;
    }
    world.scheduleBlockTick(x, y, z, wire::common::Identifier::vanilla("scaffolding"), 1);
}

ScaffoldingTurn scaffoldingTick(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || pathOf(*stateId) != "scaffolding") {
        return ScaffoldingTurn{};
    }
    const std::int32_t distance = scaffoldDistance(world, x, y, z);
    // parity: `ScaffoldingBlock.tick` decides by the **new** distance, and tells apart a piece that
    // was already at seven (it falls) from one that has just got there (it breaks).
    if (distance >= kScaffoldReach) {
        const bool wasOutOfReach = numberProperty(*stateId, "distance").value_or(0) >= kScaffoldReach;
        return ScaffoldingTurn{.falls = wasOutOfReach, .breaks = !wasOutOfReach};
    }
    // parity: `ScaffoldingBlock.isBottom`, which a piece standing on the ground never is.
    const bool bottom = distance > 0 && !isScaffolding(world, x, y - 1, z);
    const std::optional<std::int32_t> reshaped = registry::blockStateDefinitions::stateIdOf(
        wire::common::Identifier::vanilla("scaffolding"),
        {{"distance", std::to_string(distance)}, {"bottom", bottom ? "true" : "false"}}, *stateId);
    if (!reshaped.has_value() || *reshaped == *stateId || !world.setBlockState(x, y, z, *reshaped)) {
        return ScaffoldingTurn{};
    }
    return ScaffoldingTurn{.reshaped = reshaped};
}

std::optional<BlockChange> frostedIceHeard(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

void frostedIceChain(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                     std::vector<BlockChange>& changes) {
    // parity: melting is a `setBlockAndUpdate`, so each piece that goes tells its own six squares
    // with the block that **was** there, which is more frosted ice: that is how a whole trail goes at
    // once.
    std::deque<std::array<std::int32_t, 3>> pending{{std::array<std::int32_t, 3>{x, y, z}}};
    std::int32_t melted = 0;
    while (!pending.empty() && melted < kFrostedChainLimit) {
        const std::array<std::int32_t, 3> at = pending.front();
        pending.pop_front();
        for (const registry::BlockFace face : kAllFaces) {
            const std::int32_t nx = at[0] + offsetX(face);
            const std::int32_t ny = at[1] + offsetY(face);
            const std::int32_t nz = at[2] + offsetZ(face);
            if (const std::optional<BlockChange> gone = frostedIceHeard(world, nx, ny, nz); gone.has_value()) {
                changes.push_back(*gone);
                pending.push_back({nx, ny, nz});
                ++melted;
            }
        }
    }
}

std::optional<BlockChange> frostedIceHeard(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || pathOf(*stateId) != "frosted_ice") {
        return std::nullopt;
    }
    // parity: `FrostedIceBlock.neighborChanged` melts it whole when fewer than two of its six squares
    // are frosted ice too, and only when what changed is another piece of frosted ice.
    if (neighbouringIce(world, x, y, z) >= kFrostedMeltNeighbours) {
        return std::nullopt;
    }
    if (!world.setBlockState(x, y, z, waterState())) {
        return std::nullopt;
    }
    return BlockChange{.x = x, .y = y, .z = z, .stateId = waterState()};
}

void frostedIce(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, random::RandomSource& random,
                std::vector<BlockChange>& changes, bool hasSkylight) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || pathOf(*stateId) != "frosted_ice") {
        return;
    }
    const std::optional<std::int32_t> age = numberProperty(*stateId, "age");
    if (!age.has_value()) {
        return;
    }
    // parity: `FrostedIceBlock.tick`. One roll in three, **or** fewer than four frosted neighbours,
    // is what lets it look at the light at all; the roll is spent either way because it comes first.
    const bool looks = random.nextInt(3) == 0 || neighbouringIce(world, x, y, z) < kFrostedNeighbours;
    if (looks &&
        worldLight::rawBrightness(world, x, y, z, hasSkylight) >
            kMeltLight - *age - registry::blockShapes::dampening(*stateId) &&
        thin(world, x, y, z, *age, changes)) {
        // parity: only a piece that went **whole** reaches its neighbours, and it thins each of them
        // on the spot. The one that survives that thinning is the one that books a turn; the one that
        // went with it books nothing, because it is water now.
        for (const registry::BlockFace face : kAllFaces) {
            const std::int32_t nx = x + offsetX(face);
            const std::int32_t ny = y + offsetY(face);
            const std::int32_t nz = z + offsetZ(face);
            const std::optional<std::int32_t> neighbour = world.blockState(nx, ny, nz);
            if (!neighbour.has_value() || pathOf(*neighbour) != "frosted_ice") {
                continue;
            }
            const std::optional<std::int32_t> neighbourAge = numberProperty(*neighbour, "age");
            if (!neighbourAge.has_value()) {
                continue;
            }
            if (!thin(world, nx, ny, nz, *neighbourAge, changes)) {
                world.scheduleBlockTick(nx, ny, nz, frostedIceTickType(),
                                        random.nextIntBetweenInclusive(kFrostedRetryLow, kFrostedRetryHigh));
            }
        }
        return;
    }
    world.scheduleBlockTick(x, y, z, frostedIceTickType(),
                            random.nextIntBetweenInclusive(kFrostedRetryLow, kFrostedRetryHigh));
}

void driedGhast(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int64_t gameTime,
                std::vector<BlockChange>& changes) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr || block->path() != "dried_ghast") {
        return;
    }
    const std::optional<std::int32_t> hydration = numberProperty(*stateId, "hydration");
    if (!hydration.has_value()) {
        return;
    }
    const bool soaked = textProperty(*stateId, "waterlogged") == "true";
    std::int32_t wanted = 0;
    if (soaked && *hydration < kDriedGhastMax) {
        wanted = *hydration + 1;
    } else if (!soaked && *hydration > 0) {
        wanted = *hydration - 1;
    } else {
        return;
    }
    const std::optional<std::int32_t> changed =
        registry::blockStateDefinitions::stateIdOf(*block, {{"hydration", std::to_string(wanted)}}, *stateId);
    if (!changed.has_value() || !world.setBlockState(x, y, z, *changed)) {
        return;
    }
    changes.push_back(BlockChange{.x = x, .y = y, .z = z, .stateId = *changed});
    // parity: `DriedGhastBlock.tick` raises `BLOCK_CHANGE` on both sides of the clock, hydrating and
    // drying, so a sculk sensor beside one hears it change.
    if (world.hasSculkSensorNear(x, y, z, vibrations::kCalibratedRange)) {
        vibrations::emit(world, "block_change", static_cast<double>(x) + 0.5, static_cast<double>(y) + 0.5,
                         static_cast<double>(z) + 0.5, gameTime);
    }
    world.scheduleBlockTick(x, y, z, driedGhastTickType(), kDriedGhastDelay);
}

void scheduleHatch(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int64_t seed) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || pathOf(*stateId) != "sniffer_egg") {
        return;
    }
    const std::int32_t below = world.blockState(x, y - 1, z).value_or(0);
    const std::int32_t hatchTime =
        registry::blockConnections::boostsHatching(below) ? kBoostedHatchTicks : kRegularHatchTicks;
    random::RandomSource random = random::RandomSource::legacy(seed);
    const std::int32_t spread = random.nextInt(kHatchSpreadTicks);
    world.scheduleBlockTick(x, y, z, snifferEggTickType(), (hatchTime / 3) + spread);
}

void scheduleFrogspawn(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int64_t seed) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || pathOf(*stateId) != "frogspawn") {
        return;
    }
    random::RandomSource random = random::RandomSource::legacy(seed);
    const std::int32_t spread = random.nextInt(kFrogspawnMaxDelay - kFrogspawnMinDelay);
    world.scheduleBlockTick(x, y, z, frogspawnTickType(), kFrogspawnMinDelay + spread);
}

bool booksItsOwnBreak(const std::string_view path) {
    // parity: every class whose `updateShape` answers a neighbour with `scheduleTick(pos, this, 1)`
    // and whose `tick` is `if (!canSurvive) destroyBlock(pos, true)`. Which sides wake them up is
    // still `blockSurvival::loosenedBy`'s question; this is only about **when** they go.
    static constexpr std::array<std::string_view, 15> kBooks{
        "sugar_cane",          "cactus",         "big_dripleaf_stem",    "bamboo",
        "chorus_plant",        "chorus_flower",  "pale_hanging_moss",    "kelp",
        "kelp_plant",          "twisting_vines", "twisting_vines_plant", "weeping_vines",
        "weeping_vines_plant", "cave_vines",     "cave_vines_plant"};
    return std::ranges::find(kBooks, path) != kBooks.end();
}

bool isSpeleothem(const std::string_view path) { return path == "pointed_dripstone" || path == "sulfur_spike"; }

void scheduleCreakingHeart(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || pathOf(*stateId) != "creaking_heart") {
        return;
    }
    // parity: `CreakingHeartBlock.updateShape` books a turn for **any** neighbour that moved, and
    // answers the state it was given.
    world.scheduleBlockTick(x, y, z, wire::common::Identifier::vanilla("creaking_heart"), 1);
}

std::optional<std::int32_t> creakingHeartTick(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                              const IsPaleOakLog& isPaleOakLog, const bool creakingActive) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || pathOf(*stateId) != "creaking_heart" || !isPaleOakLog) {
        return std::nullopt;
    }
    // parity: `CreakingHeartBlock.updateState` only ever wakes an uprooted heart; nothing puts one
    // back to sleep from here.
    if (textProperty(*stateId, "creaking_heart_state") != "uprooted") {
        return std::nullopt;
    }
    // parity: `hasRequiredLogs`, both ends of its own axis and nothing else.
    const std::string axis = textProperty(*stateId, "axis");
    const std::int32_t stepX = axis == "x" ? 1 : 0;
    const std::int32_t stepY = axis == "y" ? 1 : 0;
    const std::int32_t stepZ = axis == "z" ? 1 : 0;
    for (const std::int32_t side : {1, -1}) {
        const std::optional<std::int32_t> end =
            world.blockState(x + (stepX * side), y + (stepY * side), z + (stepZ * side));
        if (!end.has_value() || !isPaleOakLog(*end)) {
            return std::nullopt;
        }
    }
    const std::optional<std::int32_t> woken = registry::blockStateDefinitions::stateIdOf(
        wire::common::Identifier::vanilla("creaking_heart"),
        {{"creaking_heart_state", creakingActive ? "awake" : "dormant"}}, *stateId);
    if (!woken.has_value() || *woken == *stateId || !world.setBlockState(x, y, z, *woken)) {
        return std::nullopt;
    }
    return woken;
}

void scheduleSpeleothem(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, const bool supported) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (supported || !stateId.has_value()) {
        return;
    }
    const std::string path = pathOf(*stateId);
    if (!isSpeleothem(path)) {
        return;
    }
    const wire::common::Identifier type = wire::common::Identifier::vanilla(path);
    const bool pointsDown = textProperty(*stateId, "vertical_direction") != "up";
    // parity: `SpeleothemBlock.updateShape` leaves a stalactite alone once its turn is booked, so a
    // second neighbour moving does not push the fall two ticks further out.
    if (pointsDown && world.hasScheduledBlockTick(x, y, z, type)) {
        return;
    }
    // parity: two ticks for a stalactite, one for a stalagmite.
    world.scheduleBlockTick(x, y, z, type, pointsDown ? kStalactiteDelay : kStalagmiteDelay);
}

bool isStalactiteAt(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    return stateId.has_value() && isSpeleothem(pathOf(*stateId)) &&
           textProperty(*stateId, "vertical_direction") != "up";
}

bool isStalactiteTipAt(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return false;
    }
    // parity: `SpeleothemBlock.isTip(state, true)`, which counts a merged tip as a tip.
    const std::string thickness = textProperty(*stateId, "thickness");
    return thickness == "tip" || thickness == "tip_merge";
}

SpeleothemTurn speleothemTick(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, const bool supported) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || !isSpeleothem(pathOf(*stateId))) {
        return SpeleothemTurn{};
    }
    // parity: `SpeleothemBlock.tick`: only a stalagmite is broken where it stands. Everything else
    // that got this far is a stalactite, and a stalactite comes down as entities.
    if (textProperty(*stateId, "vertical_direction") == "up") {
        return supported ? SpeleothemTurn{} : SpeleothemTurn{.destroys = true};
    }
    return SpeleothemTurn{.falls = true};
}

void scheduleBreakIfLoose(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, const bool supported) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (supported || !stateId.has_value() || !booksItsOwnBreak(pathOf(*stateId))) {
        return;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr) {
        return;
    }
    world.scheduleBlockTick(x, y, z, *block, 1);
}

void breakIfStillLoose(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, const bool supported,
                       std::vector<BlockChange>& changes) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (supported || !stateId.has_value() || !booksItsOwnBreak(pathOf(*stateId))) {
        return;
    }
    const std::int32_t empty = registry::blocks::air().stateId;
    if (!world.setBlockState(x, y, z, empty)) {
        return;
    }
    changes.push_back(BlockChange{.x = x, .y = y, .z = z, .stateId = empty});
}

void frogspawn(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::vector<BlockChange>& changes) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || pathOf(*stateId) != "frogspawn") {
        return;
    }
    const std::int32_t empty = registry::blocks::air().stateId;
    if (!world.setBlockState(x, y, z, empty)) {
        return;
    }
    changes.push_back(BlockChange{.x = x, .y = y, .z = z, .stateId = empty});
}

void snifferEgg(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, random::RandomSource& random,
                std::vector<BlockChange>& changes) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr || block->path() != "sniffer_egg") {
        return;
    }
    const std::optional<std::int32_t> hatch = numberProperty(*stateId, "hatch");
    if (!hatch.has_value()) {
        return;
    }
    if (*hatch >= kSnifferMaxHatch) {
        const std::int32_t empty = registry::blocks::air().stateId;
        if (!world.setBlockState(x, y, z, empty)) {
            return;
        }
        changes.push_back(BlockChange{.x = x, .y = y, .z = z, .stateId = empty});
        return;
    }
    const std::optional<std::int32_t> cracked =
        registry::blockStateDefinitions::stateIdOf(*block, {{"hatch", std::to_string(*hatch + 1)}}, *stateId);
    if (!cracked.has_value() || !world.setBlockState(x, y, z, *cracked)) {
        return;
    }
    changes.push_back(BlockChange{.x = x, .y = y, .z = z, .stateId = *cracked});
    scheduleHatch(world, x, y, z, random.nextLong());
}

void scheduleBubbleColumn(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> here = world.blockState(x, y, z);
    if (!here.has_value()) {
        return;
    }
    // parity: `BubbleColumnBlock.updateShape` books its own turn five ticks out, which is what
    // carries a column up and what takes it apart when its source goes.
    if (pathOf(*here) == "bubble_column") {
        world.scheduleBlockTick(x, y, z, bubbleColumnTickType(), kColumnStepDelay);
        return;
    }
    // parity: `LiquidBlock.tryScheduleBubbleBlockColumn`, twenty ticks (`BUBBLE_COLUMN_CHECK_DELAY`)
    // booked on the **water**, not on the column: a source over soul sand or magma waits a whole
    // second before the column appears. Doing it at once built the column on the spot.
    if (*here != waterState()) {
        return;
    }
    const std::string below = pathOf(world.blockState(x, y - 1, z).value_or(0));
    if (below == "soul_sand" || below == "magma_block") {
        world.scheduleBlockTick(x, y, z, wire::common::Identifier::vanilla("water"), kColumnCheckDelay);
    }
}

void bubbleColumn(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                  std::vector<BlockChange>& changes) {
    const std::optional<std::int32_t> here = world.blockState(x, y, z);
    if (!here.has_value() || !canOccupy(*here)) {
        return;
    }
    const std::optional<std::int32_t> belowState = world.blockState(x, y - 1, z);
    const std::string below = pathOf(belowState.value_or(0));
    std::optional<std::int32_t> column;
    if (below == "bubble_column") {
        column = belowState;
    } else if (below == "soul_sand") {
        column = columnState(false);
    } else if (below == "magma_block") {
        column = columnState(true);
    } else if (pathOf(*here) == "bubble_column") {
        // Nothing feeding it any more: what was a column goes back to being water.
        column = waterState();
    }
    if (!column.has_value()) {
        return;
    }
    std::int32_t level = y;
    while (true) {
        const std::optional<std::int32_t> state = world.blockState(x, level, z);
        if (!state.has_value() || !canOccupy(*state)) {
            return;
        }
        if (*state == *column) {
            ++level;
            continue;
        }
        if (!world.setBlockState(x, level, z, *column)) {
            return;
        }
        changes.push_back(BlockChange{.x = x, .y = level, .z = z, .stateId = *column});
        ++level;
    }
}

} // namespace basalt::server::runtime::world::scheduledBlocks
