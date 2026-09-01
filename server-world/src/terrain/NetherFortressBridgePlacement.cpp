#include <basalt/server/world/terrain/NetherFortressPlacement.hpp>

#include <array>
#include <utility>

namespace basalt::server::world::terrain::netherFortressPieces {
namespace {

using structure::StructureDirection;

/// parity: the piece's own spawner, whose blaze settings are written out block-entity side.
void placeSpawner(const Brush& brush, std::int32_t x, std::int32_t y, std::int32_t z) {
    if (!brush.isInside(x, y, z)) {
        return;
    }
    const std::int32_t worldX = brush.worldX(x, z);
    const std::int32_t worldY = brush.piece().boundingBox.minY + y;
    const std::int32_t worldZ = brush.worldZ(x, z);
    brush.chunk().setBlockStateId(worldX & 15, worldY, worldZ & 15, spawner());
    wire::nbt::NbtCompound blaze;
    blaze.set("id", wire::nbt::NbtTag::ofString("minecraft:blaze"));
    wire::nbt::NbtCompound spawnData;
    spawnData.set("entity", wire::nbt::NbtTag::ofCompound(std::move(blaze)));
    wire::nbt::NbtCompound data;
    data.set("id", wire::nbt::NbtTag::ofString("minecraft:mob_spawner"));
    data.set("Delay", wire::nbt::NbtTag::ofShort(20));
    data.set("MinSpawnDelay", wire::nbt::NbtTag::ofShort(200));
    data.set("MaxSpawnDelay", wire::nbt::NbtTag::ofShort(800));
    data.set("SpawnCount", wire::nbt::NbtTag::ofShort(4));
    data.set("MaxNearbyEntities", wire::nbt::NbtTag::ofShort(6));
    data.set("RequiredPlayerRange", wire::nbt::NbtTag::ofShort(16));
    data.set("SpawnRange", wire::nbt::NbtTag::ofShort(4));
    data.set("SpawnData", wire::nbt::NbtTag::ofCompound(std::move(spawnData)));
    data.set("SpawnPotentials", wire::nbt::NbtTag::ofList(wire::nbt::NbtList{wire::nbt::NbtType::End}));
    data.set("components", wire::nbt::NbtTag::ofCompound(wire::nbt::NbtCompound{}));
    brush.chunk().setBlockEntity(ChunkBlockEntity{worldX & 15, worldY, worldZ & 15,
                                                  wire::common::Identifier::vanilla("mob_spawner"), std::move(data)});
}

} // namespace

void placeBridgeCrossing(const Brush& brush) {
    brush.box(7, 3, 0, 11, 4, 18, netherBricks());
    brush.box(0, 3, 7, 18, 4, 11, netherBricks());
    brush.box(8, 5, 0, 10, 7, 18, air());
    brush.box(0, 5, 8, 18, 7, 10, air());
    for (const auto& [z0, z1] :
         std::array<std::pair<std::int32_t, std::int32_t>, 2>{std::pair{0, 7}, std::pair{11, 18}}) {
        for (const std::int32_t x : {7, 11}) {
            brush.box(x, 5, z0, x, 5, z1, netherBricks());
        }
    }
    for (const auto& [x0, x1] :
         std::array<std::pair<std::int32_t, std::int32_t>, 2>{std::pair{0, 7}, std::pair{11, 18}}) {
        for (const std::int32_t z : {7, 11}) {
            brush.box(x0, 5, z, x1, 5, z, netherBricks());
        }
    }
    for (const auto& [z0, z1] :
         std::array<std::pair<std::int32_t, std::int32_t>, 2>{std::pair{0, 5}, std::pair{13, 18}}) {
        brush.box(7, 2, z0, 11, 2, z1, netherBricks());
        brush.box(7, 0, z0 == 0 ? 0 : 15, 11, 1, z0 == 0 ? 3 : z1, netherBricks());
        for (std::int32_t x = 7; x <= 11; ++x) {
            for (std::int32_t z = 0; z <= 2; ++z) {
                brush.fillDown(x, -1, z0 == 0 ? z : 18 - z, netherBricks());
            }
        }
    }
    for (const auto& [x0, x1] :
         std::array<std::pair<std::int32_t, std::int32_t>, 2>{std::pair{0, 5}, std::pair{13, 18}}) {
        brush.box(x0, 2, 7, x1, 2, 11, netherBricks());
        brush.box(x0 == 0 ? 0 : 15, 0, 7, x0 == 0 ? 3 : x1, 1, 11, netherBricks());
        for (std::int32_t x = 0; x <= 2; ++x) {
            for (std::int32_t z = 7; z <= 11; ++z) {
                brush.fillDown(x0 == 0 ? x : 18 - x, -1, z, netherBricks());
            }
        }
    }
}

void placeBridgeEnd(const Brush& brush) {
    random::RandomSource random = random::RandomSource::worldgen(
        random::RandomSource::legacy(static_cast<std::int64_t>(brush.piece().selfSeed.value_or(0))));
    for (std::int32_t x = 0; x <= 4; ++x) {
        for (std::int32_t y = 3; y <= 4; ++y) {
            brush.box(x, y, 0, x, y, random.nextInt(8), netherBricks());
        }
    }
    brush.box(0, 5, 0, 0, 5, random.nextInt(8), netherBricks());
    brush.box(4, 5, 0, 4, 5, random.nextInt(8), netherBricks());
    for (std::int32_t x = 0; x <= 4; ++x) {
        brush.box(x, 2, 0, x, 2, random.nextInt(5), netherBricks());
    }
    for (std::int32_t x = 0; x <= 4; ++x) {
        for (std::int32_t y = 0; y <= 1; ++y) {
            brush.box(x, y, 0, x, y, random.nextInt(3), netherBricks());
        }
    }
}

void placeBridgeStraight(const Brush& brush) {
    brush.box(0, 3, 0, 4, 4, 18, netherBricks());
    brush.box(1, 5, 0, 3, 7, 18, air());
    brush.box(0, 5, 0, 0, 5, 18, netherBricks());
    brush.box(4, 5, 0, 4, 5, 18, netherBricks());
    for (const auto& [z0, z1] :
         std::array<std::pair<std::int32_t, std::int32_t>, 2>{std::pair{0, 5}, std::pair{13, 18}}) {
        brush.box(0, 2, z0, 4, 2, z1, netherBricks());
    }
    for (const auto& [z0, z1] :
         std::array<std::pair<std::int32_t, std::int32_t>, 2>{std::pair{0, 3}, std::pair{15, 18}}) {
        brush.box(0, 0, z0, 4, 1, z1, netherBricks());
    }
    for (std::int32_t x = 0; x <= 4; ++x) {
        for (std::int32_t z = 0; z <= 2; ++z) {
            brush.fillDown(x, -1, z, netherBricks());
            brush.fillDown(x, -1, 18 - z, netherBricks());
        }
    }
    const std::int32_t nseFence =
        brush.fence({StructureDirection::North, StructureDirection::South, StructureDirection::East});
    const std::int32_t nswFence =
        brush.fence({StructureDirection::North, StructureDirection::South, StructureDirection::West});
    for (const std::int32_t z : {1, 4, 14, 17}) {
        const std::int32_t minY = (z == 1 || z == 17) ? 1 : 3;
        brush.box(0, minY, z, 0, 4, z, nseFence);
        brush.box(4, minY, z, 4, 4, z, nswFence);
    }
}

void placeRoomCrossing(const Brush& brush) {
    brush.box(0, 0, 0, 6, 1, 6, netherBricks());
    brush.box(0, 2, 0, 6, 7, 6, air());
    brush.box(0, 2, 0, 1, 6, 0, netherBricks());
    brush.box(0, 2, 6, 1, 6, 6, netherBricks());
    brush.box(5, 2, 0, 6, 6, 0, netherBricks());
    brush.box(5, 2, 6, 6, 6, 6, netherBricks());
    brush.box(0, 2, 0, 0, 6, 1, netherBricks());
    brush.box(0, 2, 5, 0, 6, 6, netherBricks());
    brush.box(6, 2, 0, 6, 6, 1, netherBricks());
    brush.box(6, 2, 5, 6, 6, 6, netherBricks());
    const std::int32_t weFence = brush.fence({StructureDirection::West, StructureDirection::East});
    const std::int32_t nsFence = brush.fence({StructureDirection::North, StructureDirection::South});
    brush.box(2, 6, 0, 4, 6, 0, netherBricks());
    brush.box(2, 5, 0, 4, 5, 0, weFence);
    brush.box(2, 6, 6, 4, 6, 6, netherBricks());
    brush.box(2, 5, 6, 4, 5, 6, weFence);
    brush.box(0, 6, 2, 0, 6, 4, netherBricks());
    brush.box(0, 5, 2, 0, 5, 4, nsFence);
    brush.box(6, 6, 2, 6, 6, 4, netherBricks());
    brush.box(6, 5, 2, 6, 5, 4, nsFence);
    for (std::int32_t x = 0; x <= 6; ++x) {
        for (std::int32_t z = 0; z <= 6; ++z) {
            brush.fillDown(x, -1, z, netherBricks());
        }
    }
}

void placeStairsRoom(const Brush& brush) {
    brush.box(0, 0, 0, 6, 1, 6, netherBricks());
    brush.box(0, 2, 0, 6, 10, 6, air());
    brush.box(0, 2, 0, 1, 8, 0, netherBricks());
    brush.box(5, 2, 0, 6, 8, 0, netherBricks());
    brush.box(0, 2, 1, 0, 8, 6, netherBricks());
    brush.box(6, 2, 1, 6, 8, 6, netherBricks());
    brush.box(1, 2, 6, 5, 8, 6, netherBricks());
    const std::int32_t weFence = brush.fence({StructureDirection::West, StructureDirection::East});
    const std::int32_t nsFence = brush.fence({StructureDirection::North, StructureDirection::South});
    brush.box(0, 3, 2, 0, 5, 4, nsFence);
    brush.box(6, 3, 2, 6, 5, 2, nsFence);
    brush.box(6, 3, 4, 6, 5, 4, nsFence);
    brush.placeBlock(5, 2, 5, netherBricks());
    brush.box(4, 2, 5, 4, 3, 5, netherBricks());
    brush.box(3, 2, 5, 3, 4, 5, netherBricks());
    brush.box(2, 2, 5, 2, 5, 5, netherBricks());
    brush.box(1, 2, 5, 1, 6, 5, netherBricks());
    brush.box(1, 7, 1, 5, 7, 4, netherBricks());
    brush.box(6, 8, 2, 6, 8, 4, air());
    brush.box(2, 6, 0, 4, 8, 0, netherBricks());
    brush.box(2, 5, 0, 4, 5, 0, weFence);
    for (std::int32_t x = 0; x <= 6; ++x) {
        for (std::int32_t z = 0; z <= 6; ++z) {
            brush.fillDown(x, -1, z, netherBricks());
        }
    }
}

void placeMonsterThrone(const Brush& brush) {
    brush.box(0, 2, 0, 6, 7, 7, air());
    brush.box(1, 0, 0, 5, 1, 7, netherBricks());
    brush.box(1, 2, 1, 5, 2, 7, netherBricks());
    brush.box(1, 3, 2, 5, 3, 7, netherBricks());
    brush.box(1, 4, 3, 5, 4, 7, netherBricks());
    brush.box(1, 2, 0, 1, 4, 2, netherBricks());
    brush.box(5, 2, 0, 5, 4, 2, netherBricks());
    brush.box(1, 5, 2, 1, 5, 3, netherBricks());
    brush.box(5, 5, 2, 5, 5, 3, netherBricks());
    brush.box(0, 5, 3, 0, 5, 8, netherBricks());
    brush.box(6, 5, 3, 6, 5, 8, netherBricks());
    brush.box(1, 5, 8, 5, 5, 8, netherBricks());
    const std::int32_t weFence = brush.fence({StructureDirection::West, StructureDirection::East});
    const std::int32_t nsFence = brush.fence({StructureDirection::North, StructureDirection::South});
    brush.placeBlock(1, 6, 3, brush.fence({StructureDirection::West}));
    brush.placeBlock(5, 6, 3, brush.fence({StructureDirection::East}));
    brush.placeBlock(0, 6, 3, brush.fence({StructureDirection::East, StructureDirection::North}));
    brush.placeBlock(6, 6, 3, brush.fence({StructureDirection::West, StructureDirection::North}));
    brush.box(0, 6, 4, 0, 6, 7, nsFence);
    brush.box(6, 6, 4, 6, 6, 7, nsFence);
    brush.placeBlock(0, 6, 8, brush.fence({StructureDirection::East, StructureDirection::South}));
    brush.placeBlock(6, 6, 8, brush.fence({StructureDirection::West, StructureDirection::South}));
    brush.box(1, 6, 8, 5, 6, 8, weFence);
    brush.placeBlock(1, 7, 8, brush.fence({StructureDirection::East}));
    brush.box(2, 7, 8, 4, 7, 8, weFence);
    brush.placeBlock(5, 7, 8, brush.fence({StructureDirection::West}));
    brush.placeBlock(2, 8, 8, brush.fence({StructureDirection::East}));
    brush.placeBlock(3, 8, 8, weFence);
    brush.placeBlock(4, 8, 8, brush.fence({StructureDirection::West}));
    placeSpawner(brush, 3, 5, 5);
    for (std::int32_t x = 0; x <= 6; ++x) {
        for (std::int32_t z = 0; z <= 6; ++z) {
            brush.fillDown(x, -1, z, netherBricks());
        }
    }
}

} // namespace basalt::server::world::terrain::netherFortressPieces
