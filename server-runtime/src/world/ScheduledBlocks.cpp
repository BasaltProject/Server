#include <basalt/server/registry/BlockConnections.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockSupport.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/ScheduledBlocks.hpp>
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
constexpr std::int32_t kFrostedMaxAge = 3;
constexpr std::int32_t kFrostedNeighbours = 4;
constexpr std::int32_t kFrostedRetry = 20;
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

/// One step thinner, or gone. parity: `FrostedIceBlock.slightlyMelt`.
bool thin(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t age,
          std::vector<BlockChange>& changes) {
    if (age < kFrostedMaxAge) {
        const std::optional<std::int32_t> thinner = registry::blockStateDefinitions::stateIdOf(
            frostedIceTickType(), {{"age", std::to_string(age + 1)}}, std::nullopt);
        if (!thinner.has_value() || !world.setBlockState(x, y, z, *thinner)) {
            return false;
        }
        changes.push_back(BlockChange{.x = x, .y = y, .z = z, .stateId = *thinner});
        world.scheduleBlockTick(x, y, z, frostedIceTickType(), kFrostedRetry);
        return true;
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

std::optional<std::int32_t> coveredSoil(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return std::nullopt;
    }
    const std::string path = pathOf(*stateId);
    if (path != "farmland" && path != "dirt_path") {
        return std::nullopt;
    }
    const std::optional<std::int32_t> above = world.blockState(x, y + 1, z);
    if (!above.has_value() || !registry::blockShapes::isSolidState(*above)) {
        return std::nullopt;
    }
    // parity: `shouldMaintainFarmland`, the crop that holds tilled soil together.
    if (path == "farmland" && registry::blockConnections::maintainsFarmland(*above)) {
        return std::nullopt;
    }
    const std::int32_t dirt = namedState("dirt");
    if (!world.setBlockState(x, y, z, dirt)) {
        return std::nullopt;
    }
    return dirt;
}

std::optional<std::int32_t> coral(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return std::nullopt;
    }
    const std::string path = pathOf(*stateId);
    if (!path.ends_with("coral") && !path.ends_with("coral_block") && !path.ends_with("coral_fan") &&
        !path.ends_with("coral_wall_fan")) {
        return std::nullopt;
    }
    if (path.starts_with("dead_") || waterAbout(world, x, y, z, *stateId)) {
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
    if (random.nextInt(3) != 0 && neighbouringIce(world, x, y, z) >= kFrostedNeighbours) {
        world.scheduleBlockTick(x, y, z, frostedIceTickType(), kFrostedRetry);
        return;
    }
    if (worldLight::rawBrightness(world, x, y, z, hasSkylight) <=
        kMeltLight - *age - registry::blockShapes::dampening(*stateId)) {
        world.scheduleBlockTick(x, y, z, frostedIceTickType(), kFrostedRetry);
        return;
    }
    if (!thin(world, x, y, z, *age, changes)) {
        return;
    }
    for (const registry::BlockFace face : kAllFaces) {
        const std::int32_t nx = x + offsetX(face);
        const std::int32_t ny = y + offsetY(face);
        const std::int32_t nz = z + offsetZ(face);
        if (pathOf(world.blockState(nx, ny, nz).value_or(0)) != "frosted_ice") {
            continue;
        }
        world.scheduleBlockTick(nx, ny, nz, frostedIceTickType(), kFrostedRetry + random.nextInt(kFrostedRetry));
    }
}

void driedGhast(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::vector<BlockChange>& changes) {
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

void scheduleSugarCane(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, const bool supported) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (supported || !stateId.has_value() || pathOf(*stateId) != "sugar_cane") {
        return;
    }
    world.scheduleBlockTick(x, y, z, sugarCaneTickType(), 1);
}

void sugarCane(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, const bool supported,
               std::vector<BlockChange>& changes) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (supported || !stateId.has_value() || pathOf(*stateId) != "sugar_cane") {
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
