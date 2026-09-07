#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/ChunkHeight.hpp>
#include <basalt/server/world/structure/StructureTemplate.hpp>
#include <basalt/server/world/terrain/JungleTemplePiece.hpp>

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <utility>

namespace basalt::server::world::terrain {
namespace {

using structure::StructureBoundingBox;
using structure::StructureDirection;
using structure::StructureMirror;
using structure::StructurePos;
using structure::StructureRotation;

constexpr std::int32_t kLastX = JungleTemplePiece::kWidth - 1;
constexpr std::int32_t kLastZ = JungleTemplePiece::kDepth - 1;
constexpr std::int32_t kFloorY = 64;
constexpr float kCobblestoneChance = 0.4F;

using Properties = std::map<std::string, std::string>;

[[nodiscard]] std::int32_t state(std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    return found == nullptr ? registry::blocks::air().stateId : found->stateId;
}

[[nodiscard]] std::int32_t stateOf(std::string_view block, const Properties& properties) {
    const std::optional<std::int32_t> found =
        registry::blocks::stateIdOf(wire::common::Identifier::vanilla(std::string{block}), properties);
    return found.value_or(state(block));
}

[[nodiscard]] std::int32_t stairs(std::string_view facing) {
    return stateOf(
        "cobblestone_stairs",
        {{"facing", std::string{facing}}, {"half", "bottom"}, {"shape", "straight"}, {"waterlogged", "false"}});
}

[[nodiscard]] std::int32_t tripwireHook(std::string_view facing) {
    return stateOf("tripwire_hook", {{"facing", std::string{facing}}, {"attached", "true"}, {"powered", "false"}});
}

[[nodiscard]] std::int32_t tripwire(std::string_view first, std::string_view second) {
    Properties properties{{"attached", "true"}, {"disarmed", "false"}, {"powered", "false"}, {"north", "false"},
                          {"south", "false"},   {"east", "false"},     {"west", "false"}};
    properties[std::string{first}] = "true";
    properties[std::string{second}] = "true";
    return stateOf("tripwire", properties);
}

[[nodiscard]] std::int32_t redstone(std::initializer_list<std::pair<std::string_view, std::string_view>> sides) {
    Properties properties{{"north", "none"}, {"south", "none"}, {"east", "none"}, {"west", "none"}, {"power", "0"}};
    for (const auto& [side, value] : sides) {
        properties[std::string{side}] = std::string{value};
    }
    return stateOf("redstone_wire", properties);
}

[[nodiscard]] std::int32_t vine(std::string_view side) {
    Properties properties{
        {"north", "false"}, {"south", "false"}, {"east", "false"}, {"west", "false"}, {"up", "false"}};
    properties[std::string{side}] = "true";
    return stateOf("vine", properties);
}

[[nodiscard]] std::int32_t stickyPiston(std::string_view facing) {
    return stateOf("sticky_piston", {{"facing", std::string{facing}}, {"extended", "false"}});
}

[[nodiscard]] std::int32_t repeater(std::string_view facing) {
    return stateOf("repeater",
                   {{"facing", std::string{facing}}, {"delay", "1"}, {"locked", "false"}, {"powered", "false"}});
}

[[nodiscard]] std::int32_t air() { return registry::blocks::air().stateId; }

[[nodiscard]] std::int32_t cobblestone() {
    static const std::int32_t id = state("cobblestone");
    return id;
}

[[nodiscard]] std::int32_t mossyCobblestone() {
    static const std::int32_t id = state("mossy_cobblestone");
    return id;
}

[[nodiscard]] std::int32_t chiseledStoneBricks() {
    static const std::int32_t id = state("chiseled_stone_bricks");
    return id;
}

[[nodiscard]] std::int32_t redstoneNorthSouth() {
    static const std::int32_t id = redstone({{"north", "side"}, {"south", "side"}});
    return id;
}

[[nodiscard]] std::int32_t wallLeverNorth() {
    static const std::int32_t id = stateOf("lever", {{"facing", "north"}, {"face", "wall"}, {"powered", "false"}});
    return id;
}

constexpr std::array<std::string_view, 4> kFacings{"north", "east", "south", "west"};
constexpr std::array<std::pair<std::int32_t, std::int32_t>, 4> kFacingOffsets{std::pair{0, -1}, std::pair{1, 0},
                                                                              std::pair{0, 1}, std::pair{-1, 0}};

[[nodiscard]] std::string_view oppositeOf(std::string_view facing) {
    if (facing == "north") {
        return "south";
    }
    if (facing == "south") {
        return "north";
    }
    return facing == "east" ? "west" : "east";
}

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : block->path();
}

/// parity: `StructurePiece.reorient`.
[[nodiscard]] std::string reorient(const Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z) {
    std::string solid;
    for (std::size_t index = 0; index < kFacings.size(); ++index) {
        const auto [dx, dz] = kFacingOffsets[index];
        const std::int32_t existing = chunk.decorationBlockStateId(x + dx, y, z + dz);
        if (pathOf(existing) == "chest") {
            return "north";
        }
        if (!registry::blockShapes::isSolidRender(existing)) {
            continue;
        }
        if (!solid.empty()) {
            return "north";
        }
        solid = std::string{kFacings[index]};
    }
    return solid.empty() ? std::string{"north"} : std::string{oppositeOf(solid)};
}

/// parity: `StructurePiece.makeBoundingBox`, which swaps width and depth on the x axis.
[[nodiscard]] StructureBoundingBox makeBoundingBox(std::int32_t x, std::int32_t y, std::int32_t z,
                                                   StructureDirection orientation) {
    if (orientation == StructureDirection::North || orientation == StructureDirection::South) {
        return StructureBoundingBox{x,
                                    y,
                                    z,
                                    x + JungleTemplePiece::kWidth - 1,
                                    y + JungleTemplePiece::kHeight - 1,
                                    z + JungleTemplePiece::kDepth - 1};
    }
    return StructureBoundingBox{x,
                                y,
                                z,
                                x + JungleTemplePiece::kDepth - 1,
                                y + JungleTemplePiece::kHeight - 1,
                                z + JungleTemplePiece::kWidth - 1};
}

} // namespace

JungleTemplePiece::JungleTemplePiece(std::int32_t west, std::int32_t north, StructureDirection orientation)
    : orientation_(orientation), bounds_(makeBoundingBox(west, kFloorY, north, orientation)),
      mirror_(orientation == StructureDirection::South || orientation == StructureDirection::West
                  ? StructureMirror::LeftRight
                  : StructureMirror::None),
      rotation_(orientation == StructureDirection::West || orientation == StructureDirection::East
                    ? StructureRotation::Clockwise90
                    : StructureRotation::None) {}

bool JungleTemplePiece::settleOnGround(const std::function<std::int32_t(std::int32_t, std::int32_t)>& groundAt) {
    std::int32_t total = 0;
    std::int32_t count = 0;
    for (std::int32_t z = bounds_.minZ; z <= bounds_.maxZ; ++z) {
        for (std::int32_t x = bounds_.minX; x <= bounds_.maxX; ++x) {
            total += groundAt(x, z);
            ++count;
        }
    }
    if (count == 0) {
        return false;
    }
    bounds_ = bounds_.move(StructurePos{0, total / count - bounds_.minY, 0});
    return true;
}

void JungleTemplePiece::place(Chunk& chunk, const StructureBoundingBox& clip, random::RandomSource& random) {
    chunk_ = &chunk;
    clip_ = clip;
    random_ = &random;
    fill_ = std::make_unique<structure::StructurePieceFill>(bounds_, orientation_, chunk, clip);
    buildShell();
    buildRoof();
    buildStairs();
    buildCellar();
    buildTripwireTrap();
    buildArrowTrap();
    buildLeverPuzzle();
}

void JungleTemplePiece::set(std::int32_t stateId, std::int32_t x, std::int32_t y, std::int32_t z) const {
    fill_->place(stateId, x, y, z);
}

void JungleTemplePiece::mossBox(std::int32_t x0, std::int32_t y0, std::int32_t z0, std::int32_t x1, std::int32_t y1,
                                std::int32_t z1) const {
    for (std::int32_t y = y0; y <= y1; ++y) {
        for (std::int32_t x = x0; x <= x1; ++x) {
            for (std::int32_t z = z0; z <= z1; ++z) {
                set(random_->nextFloat() < kCobblestoneChance ? cobblestone() : mossyCobblestone(), x, y, z);
            }
        }
    }
}

void JungleTemplePiece::airBox(std::int32_t x0, std::int32_t y0, std::int32_t z0, std::int32_t x1, std::int32_t y1,
                               std::int32_t z1) const {
    for (std::int32_t y = y0; y <= y1; ++y) {
        for (std::int32_t x = x0; x <= x1; ++x) {
            for (std::int32_t z = z0; z <= z1; ++z) {
                set(air(), x, y, z);
            }
        }
    }
}

void JungleTemplePiece::buildShell() const {
    mossBox(0, -4, 0, kLastX, 0, kLastZ);
    mossBox(2, 1, 2, 9, 2, 2);
    mossBox(2, 1, 12, 9, 2, 12);
    mossBox(2, 1, 3, 2, 2, 11);
    mossBox(9, 1, 3, 9, 2, 11);
    mossBox(1, 3, 1, 10, 6, 1);
    mossBox(1, 3, 13, 10, 6, 13);
    mossBox(1, 3, 2, 1, 6, 12);
    mossBox(10, 3, 2, 10, 6, 12);
    mossBox(2, 3, 2, 9, 3, 12);
    mossBox(2, 6, 2, 9, 6, 12);
    mossBox(3, 7, 3, 8, 7, 11);
    mossBox(4, 8, 4, 7, 8, 10);
    airBox(3, 1, 3, 8, 2, 11);
    airBox(4, 3, 6, 7, 3, 9);
    airBox(2, 4, 2, 9, 5, 12);
    airBox(4, 6, 5, 7, 6, 9);
    airBox(5, 7, 6, 6, 7, 8);
    airBox(5, 1, 2, 6, 2, 2);
    airBox(5, 2, 12, 6, 2, 12);
    airBox(5, 5, 1, 6, 5, 1);
    airBox(5, 5, 13, 6, 5, 13);
    set(air(), 1, 5, 5);
    set(air(), 10, 5, 5);
    set(air(), 1, 5, 9);
    set(air(), 10, 5, 9);
}

void JungleTemplePiece::buildRoof() const {
    for (std::int32_t z = 0; z <= 14; z += 14) {
        for (const std::int32_t x : {2, 4, 7, 9}) {
            mossBox(x, 4, z, x, 5, z);
        }
    }
    mossBox(5, 6, 0, 6, 6, 0);
    for (std::int32_t x = 0; x <= 11; x += 11) {
        for (std::int32_t z = 2; z <= 12; z += 2) {
            mossBox(x, 4, z, x, 5, z);
        }
        mossBox(x, 6, 5, x, 6, 5);
        mossBox(x, 6, 9, x, 6, 9);
    }
    // Z outer, X inner: the jar writes (2,2), (9,2), (2,12), (9,12), and the selector draws in that
    // order, so swapping the loops would hand the next box a different stream position.
    for (const std::int32_t z : {2, 12}) {
        for (const std::int32_t x : {2, 9}) {
            mossBox(x, 7, z, x, 9, z);
        }
    }
    for (const std::int32_t z : {4, 10}) {
        for (const std::int32_t x : {4, 7}) {
            mossBox(x, 9, z, x, 9, z);
        }
    }
    mossBox(5, 9, 7, 6, 9, 7);
}

void JungleTemplePiece::buildStairs() const {
    set(stairs("north"), 5, 9, 6);
    set(stairs("north"), 6, 9, 6);
    set(stairs("south"), 5, 9, 8);
    set(stairs("south"), 6, 9, 8);
    for (std::int32_t x = 4; x <= 7; ++x) {
        set(stairs("north"), x, 0, 0);
    }
    for (const std::int32_t x : {4, 7}) {
        set(stairs("north"), x, 1, 8);
        set(stairs("north"), x, 2, 9);
        set(stairs("north"), x, 3, 10);
        mossBox(x, 1, 9, x, 1, 9);
    }
    mossBox(4, 1, 10, 7, 2, 10);
    mossBox(5, 4, 5, 6, 4, 5);
    set(stairs("east"), 4, 4, 5);
    set(stairs("west"), 7, 4, 5);
    // The staircase down into the cellar, cut a step at a time.
    for (std::int32_t step = 0; step < 4; ++step) {
        set(stairs("south"), 5, -step, 6 + step);
        set(stairs("south"), 6, -step, 6 + step);
        airBox(5, -step, 7 + step, 6, -step, 9 + step);
    }
}

void JungleTemplePiece::buildCellar() const {
    airBox(1, -3, 12, 10, -1, 13);
    airBox(1, -3, 1, 3, -1, 13);
    airBox(1, -3, 1, 9, -1, 5);
    for (std::int32_t z = 1; z <= 13; z += 2) {
        mossBox(1, -3, z, 1, -2, z);
    }
    for (std::int32_t z = 2; z <= 12; z += 2) {
        mossBox(1, -1, z, 3, -1, z);
    }
    mossBox(2, -2, 1, 5, -2, 1);
    mossBox(7, -2, 1, 9, -2, 1);
    mossBox(6, -3, 1, 6, -3, 1);
    mossBox(6, -1, 1, 6, -1, 1);
}

void JungleTemplePiece::buildTripwireTrap() const {
    set(tripwireHook("east"), 1, -3, 8);
    set(tripwireHook("west"), 4, -3, 8);
    set(tripwire("east", "west"), 2, -3, 8);
    set(tripwire("east", "west"), 3, -3, 8);
    for (std::int32_t z = 2; z <= 7; ++z) {
        set(redstoneNorthSouth(), 5, -3, z);
    }
    set(redstone({{"north", "side"}, {"west", "side"}}), 5, -3, 1);
    set(redstone({{"east", "side"}, {"west", "side"}}), 4, -3, 1);
    set(mossyCobblestone(), 3, -3, 1);
    createDispenser(3, -2, 1, "north");
    set(vine("south"), 3, -2, 2);
}

void JungleTemplePiece::buildArrowTrap() const {
    set(tripwireHook("north"), 7, -3, 1);
    set(tripwireHook("south"), 7, -3, 5);
    for (std::int32_t z = 2; z <= 4; ++z) {
        set(tripwire("north", "south"), 7, -3, z);
    }
    set(redstone({{"east", "side"}, {"west", "side"}}), 8, -3, 6);
    set(redstone({{"west", "side"}, {"south", "side"}}), 9, -3, 6);
    set(redstone({{"north", "side"}, {"south", "up"}}), 9, -3, 5);
    set(mossyCobblestone(), 9, -3, 4);
    set(redstoneNorthSouth(), 9, -2, 4);
    createDispenser(9, -2, 3, "west");
    set(vine("east"), 8, -1, 3);
    set(vine("east"), 8, -2, 3);
    createChest(8, -3, 3);
    set(mossyCobblestone(), 9, -3, 2);
    set(mossyCobblestone(), 8, -3, 1);
    for (const std::int32_t x : {4, 6, 8}) {
        set(mossyCobblestone(), x, -3, 5);
    }
    for (const std::int32_t x : {5, 7}) {
        set(mossyCobblestone(), x, -2, 5);
        set(mossyCobblestone(), x, -1, 5);
    }
    mossBox(9, -1, 1, 9, -1, 5);
}

void JungleTemplePiece::buildLeverPuzzle() const {
    airBox(8, -3, 8, 10, -1, 10);
    for (std::int32_t x = 8; x <= 10; ++x) {
        set(chiseledStoneBricks(), x, -2, 11);
        set(wallLeverNorth(), x, -2, 12);
    }
    mossBox(8, -3, 8, 8, -3, 10);
    mossBox(10, -3, 8, 10, -3, 10);
    set(mossyCobblestone(), 10, -2, 9);
    set(redstoneNorthSouth(), 8, -2, 9);
    set(redstoneNorthSouth(), 8, -2, 10);
    set(redstone({{"north", "side"}, {"south", "side"}, {"east", "side"}, {"west", "side"}}), 10, -1, 9);
    set(stickyPiston("up"), 9, -2, 8);
    set(stickyPiston("west"), 10, -2, 8);
    set(stickyPiston("west"), 10, -1, 8);
    set(repeater("north"), 10, -2, 10);
    createChest(9, -3, 10);
}

std::string JungleTemplePiece::transformedFacing(std::string_view facing) const {
    const std::optional<std::int32_t> stateId =
        registry::blocks::stateIdOf(wire::common::Identifier::vanilla("dispenser"),
                                    Properties{{"facing", std::string{facing}}, {"triggered", "false"}});
    if (!stateId.has_value()) {
        return std::string{facing};
    }
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    const std::int32_t turned =
        structure::StructureTemplate::transformedStateId(*stateId, properties, mirror_, rotation_);
    const std::map<std::string, std::string> turnedProperties = registry::blockStateDefinitions::propertiesOf(turned);
    const auto found = turnedProperties.find("facing");
    return found == turnedProperties.end() ? std::string{facing} : found->second;
}

void JungleTemplePiece::putLoot(std::int32_t worldX, std::int32_t worldY, std::int32_t worldZ, std::string_view type,
                                std::string_view table, std::int64_t seed) const {
    wire::nbt::NbtCompound data;
    data.set("LootTable", wire::nbt::NbtTag::ofString(std::string{table}));
    data.set("LootTableSeed", wire::nbt::NbtTag::ofLong(seed));
    data.set("components", wire::nbt::NbtTag::ofCompound(wire::nbt::NbtCompound{}));
    chunk_->setBlockEntity(ChunkBlockEntity{worldX & 15, worldY, worldZ & 15,
                                            wire::common::Identifier::vanilla(std::string{type}), std::move(data)});
}

void JungleTemplePiece::createChest(std::int32_t x, std::int32_t y, std::int32_t z) const {
    const std::int32_t worldX = fill_->worldX(x, z);
    const std::int32_t worldY = fill_->worldY(y);
    const std::int32_t worldZ = fill_->worldZ(x, z);
    if (!clip_.isInside(StructurePos{worldX, worldY, worldZ})) {
        return;
    }
    if (pathOf(chunk_->decorationBlockStateId(worldX, worldY, worldZ)) == "chest") {
        return;
    }
    // parity: the loot seed is drawn only when the chest is really placed, not before the clip.
    const std::int64_t seed = random_->nextLong();
    const Properties properties{
        {"facing", reorient(*chunk_, worldX, worldY, worldZ)}, {"type", "single"}, {"waterlogged", "false"}};
    const std::optional<std::int32_t> stateId =
        registry::blocks::stateIdOf(wire::common::Identifier::vanilla("chest"), properties);
    if (!stateId.has_value()) {
        return;
    }
    chunk_->setBlockStateId(worldX & 15, worldY, worldZ & 15, *stateId);
    putLoot(worldX, worldY, worldZ, "chest", "minecraft:chests/jungle_temple", seed);
}

void JungleTemplePiece::createDispenser(std::int32_t x, std::int32_t y, std::int32_t z, std::string_view facing) const {
    const std::int32_t worldX = fill_->worldX(x, z);
    const std::int32_t worldY = fill_->worldY(y);
    const std::int32_t worldZ = fill_->worldZ(x, z);
    if (!clip_.isInside(StructurePos{worldX, worldY, worldZ})) {
        return;
    }
    if (pathOf(chunk_->decorationBlockStateId(worldX, worldY, worldZ)) == "dispenser") {
        return;
    }
    const Properties properties{{"facing", transformedFacing(facing)}, {"triggered", "false"}};
    const std::optional<std::int32_t> stateId =
        registry::blocks::stateIdOf(wire::common::Identifier::vanilla("dispenser"), properties);
    if (!stateId.has_value()) {
        return;
    }
    chunk_->setBlockStateId(worldX & 15, worldY, worldZ & 15, *stateId);
    putLoot(worldX, worldY, worldZ, "dispenser", "minecraft:chests/jungle_temple_dispenser", random_->nextLong());
}

} // namespace basalt::server::world::terrain
