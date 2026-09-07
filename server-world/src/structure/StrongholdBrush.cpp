#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/structure/StrongholdBrush.hpp>

#include <array>
#include <cstdint>
#include <utility>

namespace basalt::server::world::structure {
namespace {

[[nodiscard]] std::map<std::string, std::string> doorProperties(std::string_view facing, std::string_view half) {
    return {{"facing", std::string{facing}},
            {"half", std::string{half}},
            {"hinge", "left"},
            {"open", "false"},
            {"powered", "false"}};
}

constexpr std::array<std::pair<std::string_view, std::pair<std::int32_t, std::int32_t>>, 4> kFacingOffsets{
    std::pair{std::string_view{"north"}, std::pair{0, -1}}, std::pair{std::string_view{"south"}, std::pair{0, 1}},
    std::pair{std::string_view{"west"}, std::pair{-1, 0}}, std::pair{std::string_view{"east"}, std::pair{1, 0}}};

[[nodiscard]] std::string oppositeFacing(std::string_view facing) {
    if (facing == "north") {
        return "south";
    }
    if (facing == "south") {
        return "north";
    }
    return facing == "west" ? "east" : "west";
}

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : block->path();
}

} // namespace

std::int32_t strongholdPalette::state(std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    return found == nullptr ? registry::blocks::air().stateId : found->stateId;
}

std::int32_t strongholdPalette::state(std::string_view path, const std::map<std::string, std::string>& properties) {
    const std::optional<std::int32_t> found =
        registry::blocks::stateIdOf(wire::common::Identifier::vanilla(std::string{path}), properties);
    return found.value_or(state(path));
}

std::int32_t strongholdPalette::stoneBricks() {
    static const std::int32_t stateId = state("stone_bricks");
    return stateId;
}

std::int32_t strongholdPalette::cracked() {
    static const std::int32_t stateId = state("cracked_stone_bricks");
    return stateId;
}

std::int32_t strongholdPalette::mossy() {
    static const std::int32_t stateId = state("mossy_stone_bricks");
    return stateId;
}

std::int32_t strongholdPalette::infested() {
    static const std::int32_t stateId = state("infested_stone_bricks");
    return stateId;
}

std::int32_t strongholdPalette::caveAir() {
    static const std::int32_t stateId = state("cave_air");
    return stateId;
}

std::int32_t strongholdPalette::smoothStoneSlab() {
    static const std::int32_t stateId = state("smooth_stone_slab");
    return stateId;
}

std::int32_t strongholdPalette::stoneBrickSlab() {
    static const std::int32_t stateId = state("stone_brick_slab");
    return stateId;
}

std::int32_t strongholdPalette::cobblestone() {
    static const std::int32_t stateId = state("cobblestone");
    return stateId;
}

std::int32_t strongholdPalette::ironDoorLower() {
    static const std::int32_t stateId = state("iron_door", doorProperties("west", "lower"));
    return stateId;
}

std::int32_t strongholdPalette::ironDoorUpper() {
    static const std::int32_t stateId = state("iron_door", doorProperties("west", "upper"));
    return stateId;
}

std::int32_t strongholdPalette::oakDoor(std::string_view facing, std::string_view half) {
    return state("oak_door", doorProperties(facing, half));
}

std::int32_t strongholdPalette::ironDoor(std::string_view facing, std::string_view half) {
    return state("iron_door", doorProperties(facing, half));
}

std::int32_t strongholdPalette::bars(std::initializer_list<std::string_view> sides) {
    const auto on = [&sides](std::string_view side) {
        return std::find(sides.begin(), sides.end(), side) != sides.end() ? "true" : "false";
    };
    return state("iron_bars", {{"north", on("north")},
                               {"south", on("south")},
                               {"east", on("east")},
                               {"west", on("west")},
                               {"waterlogged", "false"}});
}

std::int32_t strongholdPalette::stairs(std::string_view path, std::string_view facing, std::string_view half) {
    return state(path, {{"facing", std::string{facing}},
                        {"half", std::string{half}},
                        {"shape", "straight"},
                        {"waterlogged", "false"}});
}

std::int32_t strongholdPalette::wallTorch(std::string_view facing) {
    return state("wall_torch", {{"facing", std::string{facing}}});
}

std::int32_t strongholdPalette::button(std::string_view facing) {
    return state("stone_button", {{"facing", std::string{facing}}, {"face", "wall"}, {"powered", "false"}});
}

void StrongholdBrush::place(std::int32_t stateId, std::int32_t x, std::int32_t y, std::int32_t z) const {
    fill_->place(stateId, x, y, z);
}

void StrongholdBrush::box(std::int32_t x0, std::int32_t y0, std::int32_t z0, std::int32_t x1, std::int32_t y1,
                          std::int32_t z1, std::int32_t stateId) const {
    fill_->generateBox(x0, y0, z0, x1, y1, z1, stateId, stateId, false);
}

void StrongholdBrush::maybe(float chance, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) const {
    if (random_->nextFloat() < chance) {
        fill_->place(stateId, x, y, z);
    }
}

std::int32_t StrongholdBrush::smoothStone(bool isEdge) const {
    if (!isEdge) {
        return strongholdPalette::caveAir();
    }
    const float selection = random_->nextFloat();
    if (selection < 0.2F) {
        return strongholdPalette::cracked();
    }
    if (selection < 0.5F) {
        return strongholdPalette::mossy();
    }
    if (selection < 0.55F) {
        return strongholdPalette::infested();
    }
    return strongholdPalette::stoneBricks();
}

void StrongholdBrush::shell(std::int32_t x0, std::int32_t y0, std::int32_t z0, std::int32_t x1, std::int32_t y1,
                            std::int32_t z1, bool skipAir) const {
    for (std::int32_t y = y0; y <= y1; ++y) {
        for (std::int32_t x = x0; x <= x1; ++x) {
            for (std::int32_t z = z0; z <= z1; ++z) {
                if (skipAir && registry::blocks::isAir(fill_->blockAt(x, y, z))) {
                    continue;
                }
                const bool edge = y == y0 || y == y1 || x == x0 || x == x1 || z == z0 || z == z1;
                fill_->place(smoothStone(edge), x, y, z);
            }
        }
    }
}

void StrongholdBrush::frame(std::int32_t x, std::int32_t y, std::int32_t z) const {
    const std::int32_t bricks = strongholdPalette::stoneBricks();
    place(bricks, x, y, z);
    place(bricks, x, y + 1, z);
    place(bricks, x, y + 2, z);
    place(bricks, x + 1, y + 2, z);
    place(bricks, x + 2, y + 2, z);
    place(bricks, x + 2, y + 1, z);
    place(bricks, x + 2, y, z);
}

void StrongholdBrush::door(StrongholdDoor type, std::int32_t x, std::int32_t y, std::int32_t z) const {
    switch (type) {
    case StrongholdDoor::Opening:
        box(x, y, z, x + 2, y + 2, z, strongholdPalette::caveAir());
        return;
    case StrongholdDoor::WoodDoor:
        frame(x, y, z);
        place(strongholdPalette::oakDoor("north", "lower"), x + 1, y, z);
        place(strongholdPalette::oakDoor("north", "upper"), x + 1, y + 1, z);
        return;
    case StrongholdDoor::Grates:
        place(strongholdPalette::caveAir(), x + 1, y, z);
        place(strongholdPalette::caveAir(), x + 1, y + 1, z);
        place(strongholdPalette::bars({"west"}), x, y, z);
        place(strongholdPalette::bars({"west"}), x, y + 1, z);
        place(strongholdPalette::bars({"east", "west"}), x, y + 2, z);
        place(strongholdPalette::bars({"east", "west"}), x + 1, y + 2, z);
        place(strongholdPalette::bars({"east", "west"}), x + 2, y + 2, z);
        place(strongholdPalette::bars({"east"}), x + 2, y + 1, z);
        place(strongholdPalette::bars({"east"}), x + 2, y, z);
        return;
    case StrongholdDoor::IronDoor:
        frame(x, y, z);
        place(strongholdPalette::ironDoor("north", "lower"), x + 1, y, z);
        place(strongholdPalette::ironDoor("north", "upper"), x + 1, y + 1, z);
        place(strongholdPalette::button("north"), x + 2, y + 1, z + 1);
        place(strongholdPalette::button("south"), x + 2, y + 1, z - 1);
        return;
    }
}

bool StrongholdBrush::inClip(std::int32_t worldX, std::int32_t worldY, std::int32_t worldZ) const {
    return (worldX >> 4) == chunk_->chunkX() && (worldZ >> 4) == chunk_->chunkZ() &&
           worldY >= chunk_->verticalBounds().minY() && worldY <= chunk_->verticalBounds().maxY();
}

std::string StrongholdBrush::reorient(std::int32_t x, std::int32_t y, std::int32_t z) const {
    std::string solid;
    for (const auto& [direction, offset] : kFacingOffsets) {
        const std::int32_t state = chunk_->decorationBlockStateId(x + offset.first, y, z + offset.second);
        if (pathOf(state) == "chest") {
            return "north";
        }
        if (!registry::blockShapes::isSolidRender(state)) {
            continue;
        }
        if (!solid.empty()) {
            return "north";
        }
        solid = std::string{direction};
    }
    return solid.empty() ? "north" : oppositeFacing(solid);
}

void StrongholdBrush::createChest(std::int32_t x, std::int32_t y, std::int32_t z, std::string_view lootTable) const {
    const std::int32_t worldX = fill_->worldX(x, z);
    const std::int32_t worldY = fill_->worldY(y);
    const std::int32_t worldZ = fill_->worldZ(x, z);
    if (!inClip(worldX, worldY, worldZ)) {
        return;
    }
    if (pathOf(chunk_->decorationBlockStateId(worldX, worldY, worldZ)) == "chest") {
        return;
    }
    const std::int64_t seed = random_->nextLong();
    const std::int32_t stateId = strongholdPalette::state(
        "chest", {{"facing", reorient(worldX, worldY, worldZ)}, {"type", "single"}, {"waterlogged", "false"}});
    chunk_->setBlockStateId(worldX & 15, worldY, worldZ & 15, stateId);
    wire::nbt::NbtCompound data;
    data.set("LootTable", wire::nbt::NbtTag::ofString(std::string{lootTable}));
    data.set("LootTableSeed", wire::nbt::NbtTag::ofLong(seed));
    data.set("components", wire::nbt::NbtTag::ofCompound(wire::nbt::NbtCompound{}));
    chunk_->setBlockEntity(ChunkBlockEntity{worldX & 15, worldY, worldZ & 15,
                                            wire::common::Identifier::vanilla("chest"), std::move(data)});
}

void StrongholdBrush::silverfishSpawner(std::int32_t x, std::int32_t y, std::int32_t z) const {
    const std::int32_t worldX = fill_->worldX(x, z);
    const std::int32_t worldY = fill_->worldY(y);
    const std::int32_t worldZ = fill_->worldZ(x, z);
    if (!inClip(worldX, worldY, worldZ)) {
        return;
    }
    chunk_->setBlockStateId(worldX & 15, worldY, worldZ & 15, strongholdPalette::state("spawner"));
    wire::nbt::NbtCompound entity;
    entity.set("id", wire::nbt::NbtTag::ofString("minecraft:mob_spawner"));
    entity.set("Delay", wire::nbt::NbtTag::ofShort(20));
    entity.set("MinSpawnDelay", wire::nbt::NbtTag::ofShort(200));
    entity.set("MaxSpawnDelay", wire::nbt::NbtTag::ofShort(800));
    entity.set("SpawnCount", wire::nbt::NbtTag::ofShort(4));
    entity.set("MaxNearbyEntities", wire::nbt::NbtTag::ofShort(6));
    entity.set("RequiredPlayerRange", wire::nbt::NbtTag::ofShort(16));
    entity.set("SpawnRange", wire::nbt::NbtTag::ofShort(4));
    wire::nbt::NbtCompound spawnEntity;
    spawnEntity.set("id", wire::nbt::NbtTag::ofString("minecraft:silverfish"));
    wire::nbt::NbtCompound spawnData;
    spawnData.set("entity", wire::nbt::NbtTag::ofCompound(std::move(spawnEntity)));
    entity.set("SpawnData", wire::nbt::NbtTag::ofCompound(std::move(spawnData)));
    entity.set("SpawnPotentials", wire::nbt::NbtTag::ofList(wire::nbt::NbtList{wire::nbt::NbtType::End}));
    chunk_->setBlockEntity(ChunkBlockEntity{worldX & 15, worldY, worldZ & 15,
                                            wire::common::Identifier::vanilla("mob_spawner"), std::move(entity)});
}

} // namespace basalt::server::world::structure
