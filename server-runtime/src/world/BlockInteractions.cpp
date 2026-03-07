#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/BlockInteractions.hpp>
#include <basalt/server/runtime/world/RedstoneWire.hpp>

#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace basalt::server::runtime::world::blockInteractions {
namespace {

[[nodiscard]] std::string_view faceName(registry::BlockFace face) {
    switch (face) {
    case registry::BlockFace::Down:
        return "down";
    case registry::BlockFace::Up:
        return "up";
    case registry::BlockFace::North:
        return "north";
    case registry::BlockFace::South:
        return "south";
    case registry::BlockFace::West:
        return "west";
    default:
        return "east";
    }
}

using Properties = std::map<std::string, std::string>;

/// parity: `BlockSetType.canOpenByHand`, which is false for exactly one family that exists as a
/// block: iron. (`BlockSetType.GOLD` says the same, but no gold door was ever registered.)
constexpr std::string_view kIron = "iron";

/// The button families whose set uses the stone sounds and the shorter press.
constexpr std::array<std::string_view, 5> kStoneButtons{"stone", "polished_blackstone", "copper", "gold", "iron"};

/// parity: the sound sets, which are per wood rather than per block.
constexpr std::array<std::pair<std::string_view, std::string_view>, 6> kSoundSets{{
    {"bamboo", "bamboo_wood"},
    {"cherry", "cherry_wood"},
    {"crimson", "nether_wood"},
    {"warped", "nether_wood"},
    {"copper", "copper"},
    {"iron", "iron"},
}};

/// The weathering prefixes a copper block wears, which its sound set does not.
constexpr std::array<std::string_view, 4> kWeathering{"waxed_", "exposed_", "weathered_", "oxidized_"};

enum class Toggle { Door, Trapdoor, FenceGate, Lever, Button };

[[nodiscard]] std::string_view propertyOf(Toggle kind) {
    return kind == Toggle::Lever || kind == Toggle::Button ? "powered" : "open";
}

[[nodiscard]] std::string valueOf(const Properties& properties, const std::string& key) {
    const auto found = properties.find(key);
    return found == properties.end() ? std::string{} : found->second;
}

/// parity: `RepeaterBlock.DELAY` starts at one, which is what a repeater without a readable delay
/// reads as. `from_chars` leaves the value alone when it fails, so the initializer is that default.
[[nodiscard]] std::int32_t delayOf(const Properties& properties) {
    const std::string text = valueOf(properties, "delay");
    std::int32_t value = 1;
    std::from_chars(text.data(), text.data() + text.size(), value);
    return value;
}

[[nodiscard]] bool startsWithAny(std::string_view path, std::span<const std::string_view> prefixes) {
    for (const std::string_view prefix : prefixes) {
        if (path.starts_with(prefix)) {
            return true;
        }
    }
    return false;
}

/// parity: every weathering stage of a copper door shares `BlockSetType.COPPER`, so an exposed or
/// waxed one sounds like a plain copper one rather than falling back to wood.
[[nodiscard]] std::string_view familyOf(std::string_view path) {
    for (const std::string_view prefix : kWeathering) {
        if (path.starts_with(prefix)) {
            path.remove_prefix(prefix.size());
        }
    }
    return path;
}

/// parity: `Entity.getDirection`, the quarter turn a yaw falls in. Zero looks south.
[[nodiscard]] std::string_view playerFacing(float yaw) {
    static constexpr std::array<std::string_view, 4> kQuarters{"south", "west", "north", "east"};
    const auto quarter = static_cast<std::int32_t>(std::floor((yaw / 90.0F) + 0.5F));
    return kQuarters[static_cast<std::size_t>(quarter & 3)];
}

[[nodiscard]] std::string_view oppositeName(std::string_view face) {
    if (face == "north") {
        return "south";
    }
    if (face == "south") {
        return "north";
    }
    return face == "west" ? "east" : "west";
}

/// parity: `CopperGolemStatueBlock`'s four poses, in the order the enum declares them, which is what
/// `getAnalogOutputSignal` turns into one to four. The eight copper stages share the block.
constexpr std::array<std::string_view, 4> kGolemPoses{"standing", "sitting", "running", "star"};

[[nodiscard]] bool isCopperGolemStatue(std::string_view path) {
    return path == "copper_golem_statue" || path.ends_with("_copper_golem_statue");
}

[[nodiscard]] std::optional<Toggle> toggleOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return std::nullopt;
    }
    const std::string_view path = block->path();
    // parity: an iron door or trapdoor only moves for redstone, never for a hand.
    if (path == "lever") {
        return Toggle::Lever;
    }
    if (path.ends_with("_button")) {
        return Toggle::Button;
    }
    if (path.starts_with(kIron)) {
        return std::nullopt;
    }
    if (path.ends_with("_door")) {
        return Toggle::Door;
    }
    if (path.ends_with("_trapdoor")) {
        return Toggle::Trapdoor;
    }
    if (path.ends_with("_fence_gate")) {
        return Toggle::FenceGate;
    }
    return std::nullopt;
}

/// parity: `BlockSetType`'s button sounds, which do not follow its other sounds: a copper, gold or
/// iron button clicks like stone, and only the four wood families have their own.
[[nodiscard]] std::string_view buttonSoundSet(const wire::common::Identifier& block) {
    const std::string_view path = block.path();
    if (startsWithAny(path, kStoneButtons)) {
        return "stone";
    }
    if (path.starts_with("bamboo")) {
        return "bamboo_wood";
    }
    if (path.starts_with("cherry")) {
        return "cherry_wood";
    }
    if (path.starts_with("crimson") || path.starts_with("warped")) {
        return "nether_wood";
    }
    return "wooden";
}

[[nodiscard]] std::string soundFor(const wire::common::Identifier& block, Toggle kind, bool open) {
    if (kind == Toggle::Lever) {
        return "minecraft:block.lever.click";
    }
    if (kind == Toggle::Button) {
        return std::string{"minecraft:block."}.append(buttonSoundSet(block)).append("_button.click_") +
               (open ? "on" : "off");
    }
    const std::string_view action = open ? "open" : "close";
    const std::string_view family = familyOf(block.path());
    std::string_view set;
    for (const auto& [prefix, name] : kSoundSets) {
        if (family.starts_with(prefix)) {
            set = name;
            break;
        }
    }
    if (kind == Toggle::FenceGate) {
        // parity: a plain fence gate has no wood in its sound name, unlike the other two.
        return set.empty() ? std::string{"minecraft:block.fence_gate."}.append(action)
                           : std::string{"minecraft:block."}.append(set).append("_fence_gate.").append(action);
    }
    const std::string_view tail = kind == Toggle::Trapdoor ? "_trapdoor." : "_door.";
    return std::string{"minecraft:block."}.append(set.empty() ? "wooden" : set).append(tail).append(action);
}

} // namespace

bool isToggleable(std::int32_t stateId) { return toggleOf(stateId).has_value(); }

bool isCycleable(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return false;
    }
    const std::string_view path = block->path();
    // parity: `DaylightDetectorBlock.useWithoutItem` cycles `inverted`, and
    // `RedStoneWireBlock.useWithoutItem` turns a cross into a dot and back. Both are components you
    // set up by clicking them, and the bench cannot press anything, so neither was here.
    // parity: `CopperGolemStatueBlock.useItemOn` turns the statue one pose on for anything that is
    // not an axe, an empty hand included, and the pose is what a comparator beside it reads.
    return path == "repeater" || path == "comparator" || path == "daylight_detector" || isCopperGolemStatue(path) ||
           redstoneWire::isWire(stateId);
}

bool cycleYieldsToAxe(const std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block != nullptr && isCopperGolemStatue(block->path());
}

bool handToggleTellsNeighbours(const std::int32_t stateId) {
    const std::optional<Toggle> kind = toggleOf(stateId);
    return kind.has_value() && (*kind == Toggle::Lever || *kind == Toggle::Button);
}

std::optional<BlockToggle> toggle(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                  std::optional<float> playerYaw) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return std::nullopt;
    }
    const std::optional<Toggle> kind = toggleOf(*stateId);
    if (!kind.has_value()) {
        return std::nullopt;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr) {
        return std::nullopt;
    }
    const std::string property{propertyOf(*kind)};
    const Properties properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    const bool open = valueOf(properties, property) != "true";
    // parity: `ButtonBlock.useWithoutItem` does nothing to a button already down. Its own scheduled
    // tick is what lets it back up.
    if (*kind == Toggle::Button && !open) {
        return std::nullopt;
    }

    std::vector<BlockChange> changes;
    const auto flip = [&](std::int32_t atX, std::int32_t atY, std::int32_t atZ) {
        const std::optional<std::int32_t> at = world.blockState(atX, atY, atZ);
        if (!at.has_value()) {
            return;
        }
        const wire::common::Identifier* there = registry::blockStateDefinitions::blockOf(*at);
        if (there == nullptr || *there != *block) {
            return;
        }
        const std::optional<std::int32_t> flipped =
            registry::blockStateDefinitions::stateIdOf(*block, Properties{{property, open ? "true" : "false"}}, *at);
        if (!flipped.has_value() || !world.setBlockState(atX, atY, atZ, *flipped)) {
            return;
        }
        changes.push_back(BlockChange{.x = atX, .y = atY, .z = atZ, .stateId = *flipped});
    };

    // parity: `FenceGateBlock.useWithoutItem` turns a gate that is about to open and happens to face
    // **away** from whoever opened it, so it always swings out of their way. Only on the way open,
    // and only from the opposite side; from either end of its own line it is left alone.
    if (*kind == Toggle::FenceGate && open && playerYaw.has_value()) {
        const std::string_view wanted = playerFacing(*playerYaw);
        if (valueOf(properties, "facing") == oppositeName(wanted)) {
            const std::optional<std::int32_t> turned = registry::blockStateDefinitions::stateIdOf(
                *block, Properties{{"facing", std::string{wanted}}}, *stateId);
            if (turned.has_value()) {
                static_cast<void>(world.setBlockState(x, y, z, *turned));
            }
        }
    }
    flip(x, y, z);
    if (*kind == Toggle::Door) {
        // The other half, whichever way round this one is.
        const bool upper = valueOf(properties, "half") == "upper";
        flip(x, upper ? y - 1 : y + 1, z);
    }
    if (changes.empty()) {
        return std::nullopt;
    }
    return BlockToggle{.changes = std::move(changes), .sound = soundFor(*block, *kind, open)};
}

std::optional<BlockCycle> cycle(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return std::nullopt;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr) {
        return std::nullopt;
    }
    const Properties properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    const std::string_view path = block->path();
    if (path == "repeater") {
        const std::int32_t delay = delayOf(properties);
        const std::int32_t next = delay >= kMaxDelay ? 1 : delay + 1;
        const std::optional<std::int32_t> turned =
            registry::blockStateDefinitions::stateIdOf(*block, Properties{{"delay", std::to_string(next)}}, *stateId);
        if (!turned.has_value() || !world.setBlockState(x, y, z, *turned)) {
            return std::nullopt;
        }
        return BlockCycle{.stateId = *turned, .sound = std::nullopt};
    }
    if (path == "comparator") {
        const bool subtracting = valueOf(properties, "mode") == "subtract";
        const std::optional<std::int32_t> turned = registry::blockStateDefinitions::stateIdOf(
            *block, Properties{{"mode", subtracting ? "compare" : "subtract"}}, *stateId);
        if (!turned.has_value() || !world.setBlockState(x, y, z, *turned)) {
            return std::nullopt;
        }
        // parity: the pitch says which way it went, and it is quieter than a lever.
        return BlockCycle{.stateId = *turned,
                          .sound = std::string{"block.comparator.click"},
                          .pitch = subtracting ? kComparePitch : kSubtractPitch,
                          .volume = kComparatorVolume};
    }
    // parity: `CopperGolemStatueBlock.updatePose`, `POSE.getNextPose()` written with flag three. The
    // next pose wraps round, because `getNextPose` is a `ByIdMap.continuous` with `ZERO` out of
    // bounds: after `star` comes `standing` again.
    if (isCopperGolemStatue(path)) {
        const std::string pose{valueOf(properties, "copper_golem_pose")};
        const auto found = std::ranges::find(kGolemPoses, pose);
        const std::size_t next = found == kGolemPoses.end()
                                     ? 0
                                     : (static_cast<std::size_t>(found - kGolemPoses.begin()) + 1) % kGolemPoses.size();
        const std::optional<std::int32_t> turned = registry::blockStateDefinitions::stateIdOf(
            *block, Properties{{"copper_golem_pose", std::string{kGolemPoses[next]}}}, *stateId);
        if (!turned.has_value() || !world.setBlockState(x, y, z, *turned)) {
            return std::nullopt;
        }
        return BlockCycle{.stateId = *turned, .sound = std::string{"block.copper_golem_statue.become_statue"}};
    }
    if (path == "daylight_detector") {
        // parity: the detector writes `inverted` on its own with `setBlock(..., 2)` and then lets
        // `updateSignalStrength` write the new reading, which is the write that tells anybody.
        const bool inverted = valueOf(properties, "inverted") == "true";
        const std::optional<std::int32_t> turned = registry::blockStateDefinitions::stateIdOf(
            *block, Properties{{"inverted", inverted ? "false" : "true"}}, *stateId);
        if (!turned.has_value() || !world.setBlockState(x, y, z, *turned)) {
            return std::nullopt;
        }
        return BlockCycle{.stateId = *turned, .sound = std::nullopt};
    }
    if (redstoneWire::isWire(*stateId)) {
        const std::optional<std::int32_t> drawn = redstoneWire::toggleShape(world, x, y, z, *stateId);
        if (!drawn.has_value()) {
            return std::nullopt;
        }
        return BlockCycle{.stateId = *drawn, .sound = std::nullopt};
    }
    return std::nullopt;
}

std::optional<std::int32_t> pressedTicks(std::int32_t stateId) {
    if (toggleOf(stateId) != Toggle::Button) {
        return std::nullopt;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return std::nullopt;
    }
    return startsWithAny(block->path(), kStoneButtons) ? kStonePressTicks : kWoodPressTicks;
}

std::optional<BlockRelease> release(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || toggleOf(*stateId) != Toggle::Button) {
        return std::nullopt;
    }
    const Properties properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    if (valueOf(properties, "powered") != "true") {
        return std::nullopt;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr) {
        return std::nullopt;
    }
    const std::optional<std::int32_t> released =
        registry::blockStateDefinitions::stateIdOf(*block, Properties{{"powered", "false"}}, *stateId);
    if (!released.has_value() || !world.setBlockState(x, y, z, *released)) {
        return std::nullopt;
    }
    return BlockRelease{.stateId = *released, .sound = soundFor(*block, Toggle::Button, false)};
}

std::optional<std::string> redstoneSound(std::int32_t stateId, bool open) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return std::nullopt;
    }
    const std::string_view path = block->path();
    Toggle kind = Toggle::Door;
    if (path.ends_with("_trapdoor")) {
        kind = Toggle::Trapdoor;
    } else if (path.ends_with("_fence_gate")) {
        kind = Toggle::FenceGate;
    } else if (!path.ends_with("_door")) {
        return std::nullopt;
    }
    return soundFor(*block, kind, open);
}

std::optional<std::int32_t> bellShape(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                      std::int32_t stateId, registry::BlockFace towards) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr || block->path() != "bell") {
        return std::nullopt;
    }
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto attachment = properties.find("attachment");
    const auto facing = properties.find("facing");
    if (attachment == properties.end() || facing == properties.end()) {
        return std::nullopt;
    }
    static constexpr std::array<std::pair<std::string_view, registry::BlockFace>, 4> kFacings{
        {{"north", registry::BlockFace::North},
         {"south", registry::BlockFace::South},
         {"west", registry::BlockFace::West},
         {"east", registry::BlockFace::East}}};
    const auto named =
        std::ranges::find_if(kFacings, [&facing](const auto& one) { return one.first == facing->second; });
    if (named == kFacings.end() || registry::axisOf(named->second) != registry::axisOf(towards)) {
        return std::nullopt;
    }
    const std::int32_t nx = x + registry::stepX(towards);
    const std::int32_t ny = y + registry::stepY(towards);
    const std::int32_t nz = z + registry::stepZ(towards);
    const std::optional<std::int32_t> neighbour = world.blockState(nx, ny, nz);
    if (!neighbour.has_value()) {
        return std::nullopt;
    }
    // parity: the wall that has gone leaves the bell hanging on the other one, facing away from the gap.
    if (attachment->second == "double_wall" && !registry::blockShapes::isFaceSturdy(*neighbour, towards)) {
        return registry::blockStateDefinitions::stateIdOf(
            *block, {{"attachment", "single_wall"}, {"facing", std::string{faceName(registry::opposite(towards))}}},
            stateId);
    }
    // parity: and a second wall arriving on the far side joins the two.
    if (attachment->second == "single_wall" && registry::opposite(named->second) == towards &&
        registry::blockShapes::isFaceSturdy(*neighbour, named->second)) {
        return registry::blockStateDefinitions::stateIdOf(*block, {{"attachment", "double_wall"}}, stateId);
    }
    return std::nullopt;
}

std::optional<std::array<std::int32_t, 3>> attachmentOf(std::int32_t stateId, std::int32_t x, std::int32_t y,
                                                        std::int32_t z) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return std::nullopt;
    }
    const std::string_view path = block->path();
    if (path != "lever" && !path.ends_with("_button")) {
        return std::nullopt;
    }
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto face = properties.find("face");
    if (face == properties.end()) {
        return std::nullopt;
    }
    if (face->second == "floor") {
        return std::array<std::int32_t, 3>{x, y - 1, z};
    }
    if (face->second == "ceiling") {
        return std::array<std::int32_t, 3>{x, y + 1, z};
    }
    const auto facing = properties.find("facing");
    if (facing == properties.end()) {
        return std::nullopt;
    }
    static constexpr std::array<std::pair<std::string_view, std::array<std::int32_t, 3>>, 4> kBehind{
        {{"north", {0, 0, 1}}, {"south", {0, 0, -1}}, {"west", {1, 0, 0}}, {"east", {-1, 0, 0}}}};
    for (const auto& [name, step] : kBehind) {
        if (facing->second == name) {
            return std::array<std::int32_t, 3>{x + step[0], y + step[1], z + step[2]};
        }
    }
    return std::nullopt;
}

} // namespace basalt::server::runtime::world::blockInteractions
