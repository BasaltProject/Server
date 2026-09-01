#include <basalt/server/random/PositionalRandomFactory.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/ChunkHeight.hpp>
#include <basalt/server/world/terrain/DesertPyramidPiece.hpp>

#include <algorithm>
#include <array>
#include <map>
#include <set>
#include <string>
#include <utility>

namespace basalt::server::world::terrain {
namespace {

using structure::StructureBoundingBox;
using structure::StructureDirection;
using structure::StructurePos;

constexpr std::int32_t kLastX = DesertPyramidPiece::kWidth - 1;
constexpr std::int32_t kLastZ = DesertPyramidPiece::kDepth - 1;
constexpr std::int32_t kFloorY = 64;
constexpr float kCollapsedSandstoneChance = 0.33F;
constexpr std::int32_t kCellarX = 16;
constexpr std::int32_t kCellarY = -4;
constexpr std::int32_t kCellarZ = 13;

[[nodiscard]] std::int32_t state(std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    return found == nullptr ? registry::blocks::air().stateId : found->stateId;
}

[[nodiscard]] std::int32_t stairs(std::string_view facing) {
    const std::map<std::string, std::string> properties{
        {"facing", std::string{facing}}, {"half", "bottom"}, {"shape", "straight"}, {"waterlogged", "false"}};
    const std::optional<std::int32_t> found =
        registry::blocks::stateIdOf(wire::common::Identifier::vanilla("sandstone_stairs"), properties);
    return found.value_or(state("sandstone_stairs"));
}

[[nodiscard]] std::int32_t air() { return registry::blocks::air().stateId; }

[[nodiscard]] std::int32_t sandstone() {
    static const std::int32_t id = state("sandstone");
    return id;
}

[[nodiscard]] std::int32_t cutSandstone() {
    static const std::int32_t id = state("cut_sandstone");
    return id;
}

[[nodiscard]] std::int32_t chiseledSandstone() {
    static const std::int32_t id = state("chiseled_sandstone");
    return id;
}

[[nodiscard]] std::int32_t sand() {
    static const std::int32_t id = state("sand");
    return id;
}

[[nodiscard]] std::int32_t tnt() {
    static const std::int32_t id = state("tnt");
    return id;
}

[[nodiscard]] std::int32_t orangeTerracotta() {
    static const std::int32_t id = state("orange_terracotta");
    return id;
}

[[nodiscard]] std::int32_t blueTerracotta() {
    static const std::int32_t id = state("blue_terracotta");
    return id;
}

[[nodiscard]] std::int32_t stonePressurePlate() {
    static const std::int32_t id = state("stone_pressure_plate");
    return id;
}

[[nodiscard]] std::int32_t sandstoneSlab() {
    static const std::int32_t id = [] {
        const std::map<std::string, std::string> properties{{"type", "bottom"}, {"waterlogged", "false"}};
        const std::optional<std::int32_t> found =
            registry::blocks::stateIdOf(wire::common::Identifier::vanilla("sandstone_slab"), properties);
        return found.value_or(state("sandstone_slab"));
    }();
    return id;
}

/// The seven rows of the wall band, from y 2 up to y 8, each three blocks wide.
[[nodiscard]] const std::array<std::array<std::int32_t, 3>, 7>& sideRows() {
    static const std::array<std::array<std::int32_t, 3>, 7> built{{
        {cutSandstone(), orangeTerracotta(), cutSandstone()},
        {cutSandstone(), orangeTerracotta(), cutSandstone()},
        {orangeTerracotta(), chiseledSandstone(), orangeTerracotta()},
        {cutSandstone(), orangeTerracotta(), cutSandstone()},
        {orangeTerracotta(), chiseledSandstone(), orangeTerracotta()},
        {orangeTerracotta(), orangeTerracotta(), orangeTerracotta()},
        {cutSandstone(), cutSandstone(), cutSandstone()},
    }};
    return built;
}

/// parity: `Direction.Plane.HORIZONTAL`, which is north, east, south, west in that order.
constexpr std::array<std::pair<std::int32_t, std::int32_t>, 4> kHorizontalOffsets{std::pair{0, -1}, std::pair{1, 0},
                                                                                  std::pair{0, 1}, std::pair{-1, 0}};

constexpr std::array<std::string_view, 4> kFacings{"north", "east", "south", "west"};

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

[[nodiscard]] const std::set<std::string, std::less<>>& replaceable() {
    static const std::set<std::string, std::less<>> built{"water", "lava", "glow_lichen", "seagrass", "tall_seagrass"};
    return built;
}

/// parity: `StructurePiece.isReplaceableByStructures`.
[[nodiscard]] bool isReplaceableByStructures(std::int32_t stateId) {
    return registry::blocks::isAir(stateId) || replaceable().contains(pathOf(stateId));
}

/// parity: `StructurePiece.reorient`, the chest turning its back on its one solid neighbour.
[[nodiscard]] std::string reorient(const Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z) {
    std::string solid;
    for (std::size_t index = 0; index < kFacings.size(); ++index) {
        const auto [dx, dz] = kHorizontalOffsets[index];
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

} // namespace

DesertPyramidPiece::DesertPyramidPiece(std::int32_t west, std::int32_t north, StructureDirection orientation,
                                       std::int32_t groundOffset)
    : orientation_(orientation), groundOffset_(groundOffset),
      bounds_{west, kFloorY, north, west + kWidth - 1, kFloorY + kHeight - 1, north + kDepth - 1} {}

bool DesertPyramidPiece::settleOnGround(const std::function<std::int32_t(std::int32_t, std::int32_t)>& groundAt) {
    std::int32_t lowest = kOverworldHeight.maxY() + 1;
    for (std::int32_t z = bounds_.minZ; z <= bounds_.maxZ; ++z) {
        for (std::int32_t x = bounds_.minX; x <= bounds_.maxX; ++x) {
            lowest = std::min(lowest, groundAt(x, z));
        }
    }
    if (lowest > kOverworldHeight.maxY()) {
        return false;
    }
    bounds_ = bounds_.move(StructurePos{0, lowest - bounds_.minY + groundOffset_, 0});
    return true;
}

void DesertPyramidPiece::set(std::int32_t stateId, std::int32_t x, std::int32_t y, std::int32_t z) const {
    fill_->place(stateId, x, y, z);
}

void DesertPyramidPiece::box(std::int32_t x0, std::int32_t y0, std::int32_t z0, std::int32_t x1, std::int32_t y1,
                             std::int32_t z1, std::int32_t edge, std::int32_t fillWith) const {
    fill_->generateBox(x0, y0, z0, x1, y1, z1, edge, fillWith, false);
}

random::RandomSource DesertPyramidPiece::positionalAt(std::int32_t x, std::int32_t y, std::int32_t z) const {
    return random::RandomSource::legacy(worldSeed_)
        .forkPositional()
        .at(fill_->worldX(x, z), fill_->worldY(y), fill_->worldZ(x, z));
}

void DesertPyramidPiece::place(Chunk& chunk, const StructureBoundingBox& clip, std::int64_t worldSeed,
                               random::RandomSource& random) {
    chunk_ = &chunk;
    clip_ = clip;
    worldSeed_ = worldSeed;
    fill_ = std::make_unique<structure::StructurePieceFill>(bounds_, orientation_, chunk, clip);
    buildShell();
    buildTowers();
    buildEntrance();
    buildChambers();
    buildFloorMosaic();
    buildWallPattern();
    buildTrapChamber();
    placeChests(random);
    addCellarStairs();
    addCellarRoom();
}

void DesertPyramidPiece::buildShell() const {
    box(0, -4, 0, kLastX, 0, kLastZ, sandstone(), sandstone());
    for (std::int32_t layer = 1; layer <= 9; ++layer) {
        box(layer, layer, layer, kLastX - layer, layer, kLastZ - layer, sandstone(), sandstone());
        box(layer + 1, layer, layer + 1, kWidth - 2 - layer, layer, kDepth - 2 - layer, air(), air());
    }
    for (std::int32_t x = 0; x < kWidth; ++x) {
        for (std::int32_t z = 0; z < kDepth; ++z) {
            fillColumnDown(sandstone(), x, -5, z);
        }
    }
}

void DesertPyramidPiece::buildTowers() const {
    box(0, 0, 0, 4, 9, 4, sandstone(), air());
    box(1, 10, 1, 3, 10, 3, sandstone(), sandstone());
    set(stairs("north"), 2, 10, 0);
    set(stairs("south"), 2, 10, 4);
    set(stairs("east"), 0, 10, 2);
    set(stairs("west"), 4, 10, 2);
    box(kWidth - 5, 0, 0, kLastX, 9, 4, sandstone(), air());
    box(kWidth - 4, 10, 1, kWidth - 2, 10, 3, sandstone(), sandstone());
    set(stairs("north"), kWidth - 3, 10, 0);
    set(stairs("south"), kWidth - 3, 10, 4);
    set(stairs("east"), kWidth - 5, 10, 2);
    set(stairs("west"), kLastX, 10, 2);
}

void DesertPyramidPiece::buildEntrance() const {
    box(8, 0, 0, 12, 4, 4, sandstone(), air());
    box(9, 1, 0, 11, 3, 4, air(), air());
    for (const auto& [x, y] : std::array<std::pair<std::int32_t, std::int32_t>, 7>{
             std::pair{9, 1}, std::pair{9, 2}, std::pair{9, 3}, std::pair{10, 3}, std::pair{11, 3}, std::pair{11, 2},
             std::pair{11, 1}}) {
        set(cutSandstone(), x, y, 1);
    }
    box(4, 1, 1, 8, 3, 3, sandstone(), air());
    box(4, 1, 2, 8, 2, 2, air(), air());
    box(12, 1, 1, 16, 3, 3, sandstone(), air());
    box(12, 1, 2, 16, 2, 2, air(), air());
}

void DesertPyramidPiece::buildChambers() const {
    box(5, 4, 5, kWidth - 6, 4, kDepth - 6, sandstone(), sandstone());
    box(9, 4, 9, 11, 4, 11, air(), air());
    for (const std::int32_t x : {8, 12}) {
        for (const std::int32_t z : {8, 12}) {
            box(x, 1, z, x, 3, z, cutSandstone(), cutSandstone());
        }
    }
    box(1, 1, 5, 4, 4, 11, sandstone(), sandstone());
    box(kWidth - 5, 1, 5, kWidth - 2, 4, 11, sandstone(), sandstone());
    box(6, 7, 9, 6, 7, 11, sandstone(), sandstone());
    box(kWidth - 7, 7, 9, kWidth - 7, 7, 11, sandstone(), sandstone());
    box(5, 5, 9, 5, 7, 11, cutSandstone(), cutSandstone());
    box(kWidth - 6, 5, 9, kWidth - 6, 7, 11, cutSandstone(), cutSandstone());
    set(air(), 5, 5, 10);
    set(air(), 5, 6, 10);
    set(air(), 6, 6, 10);
    set(air(), kWidth - 6, 5, 10);
    set(air(), kWidth - 6, 6, 10);
    set(air(), kWidth - 7, 6, 10);
    box(2, 4, 4, 2, 6, 4, air(), air());
    box(kWidth - 3, 4, 4, kWidth - 3, 6, 4, air(), air());
    set(stairs("north"), 2, 4, 5);
    set(stairs("north"), 2, 3, 4);
    set(stairs("north"), kWidth - 3, 4, 5);
    set(stairs("north"), kWidth - 3, 3, 4);
    box(1, 1, 3, 2, 2, 3, sandstone(), sandstone());
    box(kWidth - 3, 1, 3, kWidth - 2, 2, 3, sandstone(), sandstone());
    set(sandstone(), 1, 1, 2);
    set(sandstone(), kWidth - 2, 1, 2);
    set(sandstoneSlab(), 1, 2, 2);
    set(sandstoneSlab(), kWidth - 2, 2, 2);
    set(stairs("west"), 2, 1, 2);
    set(stairs("east"), kWidth - 3, 1, 2);
    box(4, 3, 5, 4, 3, 17, sandstone(), sandstone());
    box(kWidth - 5, 3, 5, kWidth - 5, 3, 17, sandstone(), sandstone());
    box(3, 1, 5, 4, 2, 16, air(), air());
    box(kWidth - 6, 1, 5, kWidth - 5, 2, 16, air(), air());
    for (std::int32_t z = 5; z <= 17; z += 2) {
        set(cutSandstone(), 4, 1, z);
        set(chiseledSandstone(), 4, 2, z);
        set(cutSandstone(), kWidth - 5, 1, z);
        set(chiseledSandstone(), kWidth - 5, 2, z);
    }
}

void DesertPyramidPiece::buildFloorMosaic() const {
    static constexpr std::array<std::pair<std::int32_t, std::int32_t>, 12> kOrange{
        std::pair{10, 7}, std::pair{10, 8},  std::pair{9, 9},  std::pair{11, 9},  std::pair{8, 10},  std::pair{12, 10},
        std::pair{7, 10}, std::pair{13, 10}, std::pair{9, 11}, std::pair{11, 11}, std::pair{10, 12}, std::pair{10, 13}};
    for (const auto& [x, z] : kOrange) {
        set(orangeTerracotta(), x, 0, z);
    }
    set(blueTerracotta(), 10, 0, 10);
}

void DesertPyramidPiece::buildWallPattern() const {
    for (std::int32_t x = 0; x <= kLastX; x += kLastX) {
        for (std::size_t index = 0; index < sideRows().size(); ++index) {
            const std::int32_t y = static_cast<std::int32_t>(index) + 2;
            set(sideRows()[index][0], x, y, 1);
            set(sideRows()[index][1], x, y, 2);
            set(sideRows()[index][2], x, y, 3);
        }
    }
    for (std::int32_t x = 2; x <= kWidth - 3; x += kWidth - 5) {
        for (std::size_t index = 0; index < sideRows().size(); ++index) {
            const std::int32_t y = static_cast<std::int32_t>(index) + 2;
            set(sideRows()[index][0], x - 1, y, 0);
            set(sideRows()[index][1], x, y, 0);
            set(sideRows()[index][2], x + 1, y, 0);
        }
    }
    box(8, 4, 0, 12, 6, 0, cutSandstone(), cutSandstone());
    set(air(), 8, 6, 0);
    set(air(), 12, 6, 0);
    set(orangeTerracotta(), 9, 5, 0);
    set(chiseledSandstone(), 10, 5, 0);
    set(orangeTerracotta(), 11, 5, 0);
}

void DesertPyramidPiece::buildTrapChamber() const {
    box(8, -14, 8, 12, -11, 12, cutSandstone(), cutSandstone());
    box(8, -10, 8, 12, -10, 12, chiseledSandstone(), chiseledSandstone());
    box(8, -9, 8, 12, -9, 12, cutSandstone(), cutSandstone());
    box(8, -8, 8, 12, -1, 12, sandstone(), sandstone());
    box(9, -11, 9, 11, -1, 11, air(), air());
    set(stonePressurePlate(), 10, -11, 10);
    box(9, -13, 9, 11, -13, 11, tnt(), air());
    // The four alcoves, each an opening in the wall with its own two decorated blocks behind.
    for (const auto& [dx, dz] : kHorizontalOffsets) {
        set(air(), 10 + dx * 2, -11, 10 + dz * 2);
        set(air(), 10 + dx * 2, -10, 10 + dz * 2);
        set(chiseledSandstone(), 10 + dx * 3, -10, 10 + dz * 3);
        set(cutSandstone(), 10 + dx * 3, -11, 10 + dz * 3);
    }
}

void DesertPyramidPiece::placeChests(random::RandomSource& random) const {
    for (const auto& [dx, dz] : kHorizontalOffsets) {
        createChest(random, 10 + dx * 2, -11, 10 + dz * 2);
    }
}

void DesertPyramidPiece::addCellarStairs() const {
    const std::int32_t westStairs = stairs("west");
    set(westStairs, 13, -1, 17);
    set(westStairs, 14, -2, 17);
    set(westStairs, 15, -3, 17);
    const std::int32_t x = kCellarX;
    const std::int32_t y = kCellarY;
    const std::int32_t z = kCellarZ;
    random::RandomSource picker = positionalAt(x, y, z);
    const bool variant = picker.nextBoolean();
    for (std::int32_t offset = -4; offset <= 0; ++offset) {
        set(sand(), x + offset, y + 4, z + 4);
    }
    set(sand(), x - 2, y + 3, z + 4);
    set(variant ? sand() : sandstone(), x - 1, y + 3, z + 4);
    set(!variant ? sand() : sandstone(), x, y + 3, z + 4);
    set(sand(), x - 1, y + 2, z + 4);
    set(sandstone(), x, y + 2, z + 4);
    set(sand(), x, y + 1, z + 4);
}

void DesertPyramidPiece::addCellarRoom() {
    const std::int32_t x = kCellarX;
    const std::int32_t y = kCellarY;
    const std::int32_t z = kCellarZ;
    for (const auto& [level, stateId] : std::array<std::pair<std::int32_t, std::int32_t>, 3>{
             std::pair{y + 1, cutSandstone()}, std::pair{y + 2, chiseledSandstone()}, std::pair{-1, cutSandstone()}}) {
        box(x - 3, level, z - 3, x - 3, level, z + 2, stateId, stateId);
        box(x + 3, level, z - 3, x + 3, level, z + 2, stateId, stateId);
        box(x - 3, level, z - 3, x + 3, level, z - 2, stateId, stateId);
        box(x - 3, level, z + 3, x + 3, level, z + 3, stateId, stateId);
    }
    recordSandBox(x - 2, y + 1, z - 2, x + 2, y + 3, z + 2);
    placeCollapsedRoof(x - 2, y + 4, z - 2, x + 2, z + 2);
    set(blueTerracotta(), x, y, z);
    for (const std::int32_t dx : {-1, 1}) {
        for (const std::int32_t dz : {-1, 1}) {
            set(orangeTerracotta(), x + dx, y, z + dz);
        }
    }
    set(orangeTerracotta(), x + 2, y, z);
    set(orangeTerracotta(), x - 2, y, z);
    set(orangeTerracotta(), x, y, z + 2);
    set(orangeTerracotta(), x, y, z - 2);
    // The two side niches, each an orange arm with a sand plug and a decorated back.
    for (const std::int32_t dx : {1, -1}) {
        set(orangeTerracotta(), x + dx * 3, y, z);
        recordSand(x + dx * 3, y + 1, z);
        recordSand(x + dx * 3, y + 2, z);
        set(cutSandstone(), x + dx * 4, y + 1, z);
        set(chiseledSandstone(), x + dx * 4, y + 2, z);
    }
    // The two on the Z axis are not symmetric: only the north one gets a back wall.
    for (const std::int32_t dz : {1, -1}) {
        set(orangeTerracotta(), x, y, z + dz * 3);
        recordSand(x, y + 1, z + dz * 3);
        recordSand(x, y + 2, z + dz * 3);
    }
    set(cutSandstone(), x, y + 1, z - 4);
    set(chiseledSandstone(), x, -2, z - 4);
}

void DesertPyramidPiece::placeCollapsedRoof(std::int32_t x0, std::int32_t y0, std::int32_t z0, std::int32_t x1,
                                            std::int32_t z1) {
    for (std::int32_t x = x0; x <= x1; ++x) {
        for (std::int32_t z = z0; z <= z1; ++z) {
            random::RandomSource picker = positionalAt(x, y0, z);
            set(picker.nextFloat() < kCollapsedSandstoneChance ? sandstone() : sand(), x, y0, z);
        }
    }
    random::RandomSource random = random::RandomSource::legacy(worldSeed_)
                                      .forkPositional()
                                      .at(fill_->worldX(x0, z0), fill_->worldY(y0), fill_->worldZ(x0, z0));
    const std::int32_t roofX = random.nextIntBetweenInclusive(x0, x1);
    const std::int32_t roofZ = random.nextIntBetweenInclusive(z0, z1);
    collapsedRoof_ = StructurePos{fill_->worldX(roofX, roofZ), fill_->worldY(y0), fill_->worldZ(roofX, roofZ)};
}

void DesertPyramidPiece::recordSandBox(std::int32_t x0, std::int32_t y0, std::int32_t z0, std::int32_t x1,
                                       std::int32_t y1, std::int32_t z1) {
    for (std::int32_t y = y0; y <= y1; ++y) {
        for (std::int32_t x = x0; x <= x1; ++x) {
            for (std::int32_t z = z0; z <= z1; ++z) {
                recordSand(x, y, z);
            }
        }
    }
}

void DesertPyramidPiece::recordSand(std::int32_t x, std::int32_t y, std::int32_t z) {
    suspiciousSand_.push_back(StructurePos{fill_->worldX(x, z), fill_->worldY(y), fill_->worldZ(x, z)});
}

void DesertPyramidPiece::createChest(random::RandomSource& random, std::int32_t x, std::int32_t y,
                                     std::int32_t z) const {
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
    const std::int64_t lootSeed = random.nextLong();
    const std::map<std::string, std::string> properties{
        {"facing", reorient(*chunk_, worldX, worldY, worldZ)}, {"type", "single"}, {"waterlogged", "false"}};
    const std::optional<std::int32_t> stateId =
        registry::blocks::stateIdOf(wire::common::Identifier::vanilla("chest"), properties);
    if (!stateId.has_value()) {
        return;
    }
    chunk_->setBlockStateId(worldX & 15, worldY, worldZ & 15, *stateId);
    wire::nbt::NbtCompound data;
    data.set("LootTable", wire::nbt::NbtTag::ofString("minecraft:chests/desert_pyramid"));
    data.set("LootTableSeed", wire::nbt::NbtTag::ofLong(lootSeed));
    data.set("components", wire::nbt::NbtTag::ofCompound(wire::nbt::NbtCompound{}));
    chunk_->setBlockEntity(ChunkBlockEntity{worldX & 15, worldY, worldZ & 15,
                                            wire::common::Identifier::vanilla("chest"), std::move(data)});
}

void DesertPyramidPiece::fillColumnDown(std::int32_t stateId, std::int32_t x, std::int32_t startY,
                                        std::int32_t z) const {
    const std::int32_t worldX = fill_->worldX(x, z);
    const std::int32_t worldZ = fill_->worldZ(x, z);
    if ((worldX >> 4) != chunk_->chunkX() || (worldZ >> 4) != chunk_->chunkZ()) {
        return;
    }
    const std::int32_t localX = worldX & 15;
    const std::int32_t localZ = worldZ & 15;
    std::int32_t y = fill_->worldY(startY);
    while (y > kOverworldHeight.minY() + 1 && isReplaceableByStructures(chunk_->blockStateId(localX, y, localZ))) {
        chunk_->setBlockStateId(localX, y, localZ, stateId);
        --y;
    }
}

} // namespace basalt::server::world::terrain
