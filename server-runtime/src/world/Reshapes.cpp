#include <basalt/server/registry/BlockConnections.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/Reshapes.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace basalt::server::runtime::world::reshapes {
namespace {

constexpr std::array<registry::BlockFace, 6> kAllFaces{registry::BlockFace::Down,  registry::BlockFace::Up,
                                                       registry::BlockFace::North, registry::BlockFace::South,
                                                       registry::BlockFace::West,  registry::BlockFace::East};

constexpr std::array<registry::BlockFace, 4> kHorizontal{registry::BlockFace::North, registry::BlockFace::South,
                                                         registry::BlockFace::West, registry::BlockFace::East};

[[nodiscard]] std::string_view nameOf(registry::BlockFace face) {
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

[[nodiscard]] registry::BlockFace faceOf(std::string_view facing) {
    if (facing == "north") {
        return registry::BlockFace::North;
    }
    if (facing == "south") {
        return registry::BlockFace::South;
    }
    return facing == "west" ? registry::BlockFace::West : registry::BlockFace::East;
}

[[nodiscard]] registry::BlockFace oppositeFace(registry::BlockFace face) {
    switch (face) {
    case registry::BlockFace::Down:
        return registry::BlockFace::Up;
    case registry::BlockFace::Up:
        return registry::BlockFace::Down;
    case registry::BlockFace::North:
        return registry::BlockFace::South;
    case registry::BlockFace::South:
        return registry::BlockFace::North;
    case registry::BlockFace::West:
        return registry::BlockFace::East;
    default:
        return registry::BlockFace::West;
    }
}

[[nodiscard]] std::string opposite(const std::string& facing) {
    if (facing == "north") {
        return "south";
    }
    if (facing == "south") {
        return "north";
    }
    return facing == "west" ? "east" : "west";
}

[[nodiscard]] std::string clockwise(const std::string& facing) {
    if (facing == "north") {
        return "east";
    }
    if (facing == "east") {
        return "south";
    }
    return facing == "south" ? "west" : "north";
}

[[nodiscard]] std::string counterClockwise(const std::string& facing) {
    return clockwise(clockwise(clockwise(facing)));
}

[[nodiscard]] std::int32_t stateAt(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    return world.blockState(x, y, z).value_or(registry::blocks::air().stateId);
}

[[nodiscard]] std::string propertyOf(std::int32_t stateId, const std::string& name) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(name);
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : std::string{block->path()};
}

[[nodiscard]] std::optional<std::int32_t> apply(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                std::int32_t stateId, std::int32_t reshaped) {
    if (reshaped == stateId || !world.setBlockState(x, y, z, reshaped)) {
        return std::nullopt;
    }
    return reshaped;
}

[[nodiscard]] std::optional<std::int32_t> changed(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                  std::int32_t stateId, const wire::common::Identifier& block,
                                                  const std::string& property, const std::string& value) {
    const std::optional<std::int32_t> reshaped =
        registry::blockStateDefinitions::stateIdOf(block, {{property, value}}, stateId);
    if (!reshaped.has_value()) {
        return std::nullopt;
    }
    return apply(world, x, y, z, stateId, *reshaped);
}

/// parity: `AttachedStemBlock.updateShape`: pick the melon and the stem it fed goes back to being a
/// grown stem, pointing nowhere.
[[nodiscard]] std::optional<std::int32_t> detachedStem(WorldState& world, std::int32_t x, std::int32_t y,
                                                       std::int32_t z, std::int32_t stateId) {
    const std::string facing = propertyOf(stateId, "facing");
    if (facing.empty()) {
        return std::nullopt;
    }
    // parity: each `AttachedStemBlock`'s own `fruit`.
    const std::string path = pathOf(stateId);
    std::string fruit;
    if (path == "attached_melon_stem") {
        fruit = "melon";
    } else if (path == "attached_pumpkin_stem") {
        fruit = "pumpkin";
    } else {
        return std::nullopt;
    }
    const registry::BlockFace face = faceOf(facing);
    const std::int32_t neighbour = stateAt(world, x + offsetX(face), y, z + offsetZ(face));
    if (pathOf(neighbour) == fruit) {
        return std::nullopt;
    }
    const std::optional<std::int32_t> detached = registry::blockStateDefinitions::stateIdOf(
        wire::common::Identifier::vanilla(fruit + "_stem"), {{"age", "7"}}, std::nullopt);
    if (!detached.has_value()) {
        return std::nullopt;
    }
    return apply(world, x, y, z, stateId, *detached);
}

/// parity: `FenceGateBlock.updateShape`, which sinks a gate into a wall beside it.
[[nodiscard]] std::optional<std::int32_t> gate(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                               std::int32_t stateId, const wire::common::Identifier& block) {
    const std::string facing = propertyOf(stateId, "facing");
    if (facing.empty()) {
        return std::nullopt;
    }
    // The gate stands across its facing, so the wall it sinks into is on the other axis.
    const bool acrossX = facing == "north" || facing == "south";
    const std::array<registry::BlockFace, 2> sides =
        acrossX ? std::array{registry::BlockFace::West, registry::BlockFace::East}
                : std::array{registry::BlockFace::North, registry::BlockFace::South};
    bool inWall = false;
    for (const registry::BlockFace face : sides) {
        inWall = inWall || registry::blockConnections::isWall(stateAt(world, x + offsetX(face), y, z + offsetZ(face)));
    }
    return changed(world, x, y, z, stateId, block, "in_wall", inWall ? "true" : "false");
}

/// parity: `BedBlock.updateShape`: a half whose other half is gone goes with it, and one that still
/// has it copies whether it is occupied.
[[nodiscard]] std::optional<std::int32_t> bed(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                              std::int32_t stateId, const wire::common::Identifier& block) {
    const std::string facing = propertyOf(stateId, "facing");
    const std::string part = propertyOf(stateId, "part");
    if (facing.empty() || part.empty()) {
        return std::nullopt;
    }
    const registry::BlockFace face = faceOf(part == "foot" ? facing : opposite(facing));
    const std::int32_t other = stateAt(world, x + offsetX(face), y, z + offsetZ(face));
    const wire::common::Identifier* otherBlock = registry::blockStateDefinitions::blockOf(other);
    if (otherBlock == nullptr || *otherBlock != block || propertyOf(other, "part") == part) {
        const std::int32_t empty = registry::blocks::air().stateId;
        return world.setBlockState(x, y, z, empty) ? std::optional{empty} : std::nullopt;
    }
    const std::string occupied = propertyOf(other, "occupied");
    if (occupied.empty()) {
        return std::nullopt;
    }
    return changed(world, x, y, z, stateId, block, "occupied", occupied);
}

/// parity: `DoorBlock.updateShape`: each half copies the other's whole state but for its own half,
/// which is what keeps a door open on both halves and closes it on both.
[[nodiscard]] std::optional<std::int32_t> door(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                               std::int32_t stateId, const wire::common::Identifier& block,
                                               std::optional<registry::BlockFace> towards) {
    const std::string half = propertyOf(stateId, "half");
    if (half.empty()) {
        return std::nullopt;
    }
    // parity: `DoorBlock.updateShape` only comes apart when the news arrives **along its own axis and
    // from the side its other half is on**: `half == LOWER` listens up and `half == UPPER` listens
    // down. From anywhere else a lone half stays put, which is what a command can leave behind.
    const registry::BlockFace mine = half == "lower" ? registry::BlockFace::Up : registry::BlockFace::Down;
    if (!towards.has_value() || *towards != mine) {
        return std::nullopt;
    }
    const std::int32_t other = stateAt(world, x, half == "lower" ? y + 1 : y - 1, z);
    const wire::common::Identifier* otherBlock = registry::blockStateDefinitions::blockOf(other);
    if (otherBlock == nullptr || *otherBlock != block || propertyOf(other, "half") == half) {
        const std::int32_t empty = registry::blocks::air().stateId;
        return world.setBlockState(x, y, z, empty) ? std::optional{empty} : std::nullopt;
    }
    const std::optional<std::int32_t> wanted =
        registry::blockStateDefinitions::stateIdOf(block, {{"half", half}}, other);
    if (!wanted.has_value()) {
        return std::nullopt;
    }
    return apply(world, x, y, z, stateId, *wanted);
}

/// The other half of a double chest.
[[nodiscard]] std::string otherHalf(const std::string& type) { return type == "left" ? "right" : "left"; }

/// parity: `ChestBlock.updateShape`, which is only half of the pairing: it follows a neighbour that
/// has already claimed the side facing it, and it goes back to single when the chest it was paired
/// with is gone. Two chests becoming a double one is decided when the second is placed.
[[nodiscard]] std::optional<std::int32_t> chest(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                std::int32_t stateId, const wire::common::Identifier& block) {
    const std::string facing = propertyOf(stateId, "facing");
    const std::string type = propertyOf(stateId, "type");
    if (facing.empty() || type.empty()) {
        return std::nullopt;
    }
    if (type == "single") {
        for (const auto& [side, mine] : {std::pair{counterClockwise(facing), std::string{"right"}},
                                         std::pair{clockwise(facing), std::string{"left"}}}) {
            const registry::BlockFace face = faceOf(side);
            const std::int32_t neighbour = stateAt(world, x + offsetX(face), y, z + offsetZ(face));
            const wire::common::Identifier* neighbourBlock = registry::blockStateDefinitions::blockOf(neighbour);
            if (neighbourBlock == nullptr || *neighbourBlock != block) {
                continue;
            }
            if (propertyOf(neighbour, "facing") != facing || propertyOf(neighbour, "type") != otherHalf(mine)) {
                continue;
            }
            return changed(world, x, y, z, stateId, block, "type", mine);
        }
        return std::nullopt;
    }
    const registry::BlockFace face = faceOf(type == "left" ? clockwise(facing) : counterClockwise(facing));
    const std::int32_t partner = stateAt(world, x + offsetX(face), y, z + offsetZ(face));
    const wire::common::Identifier* partnerBlock = registry::blockStateDefinitions::blockOf(partner);
    const bool stillThere = partnerBlock != nullptr && *partnerBlock == block &&
                            propertyOf(partner, "facing") == facing && propertyOf(partner, "type") == otherHalf(type);
    if (stillThere) {
        return std::nullopt;
    }
    return changed(world, x, y, z, stateId, block, "type", "single");
}

/// parity: `ChorusPlantBlock.canSurvive`, word for word.
[[nodiscard]] bool chorusPlantSurvives(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::int32_t below = stateAt(world, x, y - 1, z);
    const std::int32_t air = registry::blocks::air().stateId;
    const bool blockAboveOrBelow = stateAt(world, x, y + 1, z) != air && below != air;
    for (const registry::BlockFace face : {registry::BlockFace::North, registry::BlockFace::South,
                                           registry::BlockFace::West, registry::BlockFace::East}) {
        const std::int32_t sideX = x + offsetX(face);
        const std::int32_t sideZ = z + offsetZ(face);
        if (pathOf(stateAt(world, sideX, y, sideZ)) != "chorus_plant") {
            continue;
        }
        if (blockAboveOrBelow) {
            return false;
        }
        // parity: `#minecraft:supports_chorus_plant`, which is end stone and little else.
        const std::string under = pathOf(stateAt(world, sideX, y - 1, sideZ));
        if (under == "chorus_plant" || under == "end_stone") {
            return true;
        }
    }
    const std::string underMe = pathOf(below);
    return underMe == "chorus_plant" || underMe == "end_stone";
}

/// parity: `ChorusPlantBlock.updateShape`, six faces of it.
[[nodiscard]] std::optional<std::int32_t> chorusPlant(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                      std::int32_t stateId, const wire::common::Identifier& block) {
    // parity: the shape update asks `canSurvive` **first**, and a plant that cannot stay is left
    // exactly as it is while its turn is booked: it keeps the `down` it had for one more tick.
    // Measured in `tallostorre`.
    if (!chorusPlantSurvives(world, x, y, z)) {
        return std::nullopt;
    }
    std::map<std::string, std::string> wanted;
    for (const registry::BlockFace face : kAllFaces) {
        const std::int32_t neighbour = stateAt(world, x + offsetX(face), y + offsetY(face), z + offsetZ(face));
        const std::string path = pathOf(neighbour);
        const bool connects = path == "chorus_plant" || path == "chorus_flower" ||
                              (face == registry::BlockFace::Down && path == "end_stone");
        wanted.emplace(nameOf(face), connects ? "true" : "false");
    }
    const std::optional<std::int32_t> reshaped = registry::blockStateDefinitions::stateIdOf(block, wanted, stateId);
    if (!reshaped.has_value()) {
        return std::nullopt;
    }
    return apply(world, x, y, z, stateId, *reshaped);
}

/// parity: `CampfireBlock.updateShape`: a hay bale under it turns it into a signal fire.
[[nodiscard]] std::optional<std::int32_t> campfire(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                   std::int32_t stateId, const wire::common::Identifier& block) {
    const bool signal = pathOf(stateAt(world, x, y - 1, z)) == "hay_block";
    return changed(world, x, y, z, stateId, block, "signal_fire", signal ? "true" : "false");
}

/// parity: `HangingMossBlock.updateShape`, whose tip is the strand with nothing under it.
[[nodiscard]] std::optional<std::int32_t> hangingMoss(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                      std::int32_t stateId, const wire::common::Identifier& block) {
    const wire::common::Identifier* below = registry::blockStateDefinitions::blockOf(stateAt(world, x, y - 1, z));
    const bool tip = below == nullptr || *below != block;
    return changed(world, x, y, z, stateId, block, "tip", tip ? "true" : "false");
}

[[nodiscard]] std::string thicknessOf(std::int32_t stateId) { return propertyOf(stateId, "thickness"); }

[[nodiscard]] bool isSpeleothem(std::int32_t stateId, const std::string& spike, bool pointsDown) {
    if (pathOf(stateId) != spike) {
        return false;
    }
    return (propertyOf(stateId, "vertical_direction") != "up") == pointsDown;
}

/// parity: `SpeleothemBlock.calculateSpeleothemThickness`, whose answer depends on what the piece in
/// front already is and not only on whether there is one: a spike hanging off a tip is a frustum,
/// and the one against the ceiling is only the base while nothing is behind it.
///
/// A tip meeting another tip head on merges, which is the pair that catches a drop of water.
[[nodiscard]] std::optional<std::int32_t> dripstoneThickness(WorldState& world, std::int32_t x, std::int32_t y,
                                                             std::int32_t z, std::int32_t stateId,
                                                             const wire::common::Identifier& block) {
    const std::string spike = pathOf(stateId);
    if (spike.empty()) {
        return std::nullopt;
    }
    const bool down = propertyOf(stateId, "vertical_direction") != "up";
    const std::int32_t inFront = stateAt(world, x, down ? y - 1 : y + 1, z);
    const std::int32_t behind = stateAt(world, x, down ? y + 1 : y - 1, z);
    const bool merging = propertyOf(stateId, "thickness") == "tip_merge";
    std::string thickness;
    if (isSpeleothem(inFront, spike, !down)) {
        thickness = !merging && thicknessOf(inFront) != "tip_merge" ? "tip" : "tip_merge";
    } else if (!isSpeleothem(inFront, spike, down)) {
        thickness = "tip";
    } else if (thicknessOf(inFront) != "tip" && thicknessOf(inFront) != "tip_merge") {
        thickness = !isSpeleothem(behind, spike, down) ? "base" : "middle";
    } else {
        thickness = "frustum";
    }
    return changed(world, x, y, z, stateId, block, "thickness", thickness);
}

[[nodiscard]] bool isCarpet(std::int32_t stateId) { return pathOf(stateId) == "pale_moss_carpet"; }

[[nodiscard]] bool isCarpetWith(std::int32_t stateId, const std::string& face) {
    return isCarpet(stateId) && propertyOf(stateId, face) != "none";
}

/// parity: `MossyCarpetBlock.getUpdatedState`: a side keeps standing while there is a wall to hold
/// it, grows tall under another carpet's side, and drops when the carpet below has none. A carpet
/// left with no sides at all and no base is gone.
[[nodiscard]] std::optional<std::int32_t> mossCarpet(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                     std::int32_t stateId, const wire::common::Identifier& block) {
    const bool base = propertyOf(stateId, "bottom") == "true";
    const std::int32_t above = stateAt(world, x, y + 1, z);
    const std::int32_t below = stateAt(world, x, y - 1, z);
    std::map<std::string, std::string> wanted;
    bool standing = base;
    for (const registry::BlockFace face : kHorizontal) {
        const std::string name{nameOf(face)};
        const std::int32_t neighbour = stateAt(world, x + offsetX(face), y + offsetY(face), z + offsetZ(face));
        std::string side = "none";
        if (registry::blockShapes::isFaceSturdy(neighbour, oppositeFace(face))) {
            const std::string current = propertyOf(stateId, name);
            side = current.empty() ? "none" : current;
        }
        if (side == "low") {
            if (isCarpetWith(above, name) && propertyOf(above, "bottom") != "true") {
                side = "tall";
            }
            if (!base && isCarpet(below) && propertyOf(below, name) == "none") {
                side = "none";
            }
        }
        if (side != "none") {
            standing = true;
        }
        wanted.emplace(name, side);
    }
    if (!standing) {
        return apply(world, x, y, z, stateId, registry::blocks::air().stateId);
    }
    const std::optional<std::int32_t> reshaped = registry::blockStateDefinitions::stateIdOf(block, wanted, stateId);
    if (!reshaped.has_value()) {
        return std::nullopt;
    }
    return apply(world, x, y, z, stateId, *reshaped);
}

/// parity: `HugeMushroomBlock.updateShape`: a face touching another cap stops being drawn.
[[nodiscard]] std::optional<std::int32_t> hugeMushroom(WorldState& world, std::int32_t x, std::int32_t y,
                                                       std::int32_t z, std::int32_t stateId,
                                                       const wire::common::Identifier& block) {
    std::map<std::string, std::string> wanted;
    for (const registry::BlockFace face : kAllFaces) {
        const std::int32_t neighbour = stateAt(world, x + offsetX(face), y + offsetY(face), z + offsetZ(face));
        const wire::common::Identifier* neighbourBlock = registry::blockStateDefinitions::blockOf(neighbour);
        const bool drawn = neighbourBlock == nullptr || *neighbourBlock != block;
        wanted.emplace(nameOf(face), drawn ? "true" : "false");
    }
    const std::optional<std::int32_t> reshaped = registry::blockStateDefinitions::stateIdOf(block, wanted, stateId);
    if (!reshaped.has_value()) {
        return std::nullopt;
    }
    return apply(world, x, y, z, stateId, *reshaped);
}

/// parity: `BellBlock.updateShape`, which pairs a wall bell with the wall on its other side.
[[nodiscard]] std::optional<std::int32_t> bell(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                               std::int32_t stateId, const wire::common::Identifier& block) {
    const std::string attachment = propertyOf(stateId, "attachment");
    if (attachment != "single_wall" && attachment != "double_wall") {
        return std::nullopt;
    }
    const std::string facing = propertyOf(stateId, "facing");
    if (facing.empty()) {
        return std::nullopt;
    }
    const registry::BlockFace front = faceOf(facing);
    const registry::BlockFace back = faceOf(opposite(facing));
    const bool frontSturdy =
        registry::blockShapes::isFaceSturdy(stateAt(world, x + offsetX(front), y, z + offsetZ(front)), back);
    const bool backSturdy =
        registry::blockShapes::isFaceSturdy(stateAt(world, x + offsetX(back), y, z + offsetZ(back)), front);
    return changed(world, x, y, z, stateId, block, "attachment",
                   frontSturdy && backSturdy ? "double_wall" : "single_wall");
}

[[nodiscard]] bool endsWith(std::string_view text, std::string_view suffix) { return text.ends_with(suffix); }

} // namespace

std::span<const std::string_view> chestBlocks() {
    static const std::vector<std::string_view> blocks = [] {
        std::vector<std::string_view> built{"chest", "trapped_chest"};
        static const std::array<std::string_view, 8> copper{"copper_chest",           "waxed_copper_chest",
                                                            "exposed_copper_chest",   "waxed_exposed_copper_chest",
                                                            "weathered_copper_chest", "waxed_weathered_copper_chest",
                                                            "oxidized_copper_chest",  "waxed_oxidized_copper_chest"};
        built.insert(built.end(), copper.begin(), copper.end());
        return built;
    }();
    return blocks;
}

std::optional<std::int32_t> of(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
                               const GeyserReshape& geyser, std::optional<registry::BlockFace> towards) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return std::nullopt;
    }
    const std::string_view path = block->path();
    if (endsWith(path, "_fence_gate")) {
        return gate(world, x, y, z, stateId, *block);
    }
    if (endsWith(path, "_bed")) {
        return bed(world, x, y, z, stateId, *block);
    }
    if (endsWith(path, "_door")) {
        return door(world, x, y, z, stateId, *block, towards);
    }
    if (std::ranges::find(chestBlocks(), path) != chestBlocks().end()) {
        return chest(world, x, y, z, stateId, *block);
    }
    if (path == "chorus_plant") {
        return chorusPlant(world, x, y, z, stateId, *block);
    }
    if (path == "campfire" || path == "soul_campfire") {
        return campfire(world, x, y, z, stateId, *block);
    }
    if (path == "pale_hanging_moss") {
        return hangingMoss(world, x, y, z, stateId, *block);
    }
    if (path == "pointed_dripstone" || path == "sulfur_spike") {
        return dripstoneThickness(world, x, y, z, stateId, *block);
    }
    if (endsWith(path, "mushroom_block") || path == "mushroom_stem") {
        return hugeMushroom(world, x, y, z, stateId, *block);
    }
    if (path == "bell") {
        return bell(world, x, y, z, stateId, *block);
    }
    if (path.starts_with("attached_") && endsWith(path, "_stem")) {
        return detachedStem(world, x, y, z, stateId);
    }
    if (path == "potent_sulfur") {
        return geyser ? geyser(x, y, z, stateId) : std::nullopt;
    }
    if (path == "pale_moss_carpet") {
        return mossCarpet(world, x, y, z, stateId, *block);
    }
    return std::nullopt;
}

} // namespace basalt::server::runtime::world::reshapes
