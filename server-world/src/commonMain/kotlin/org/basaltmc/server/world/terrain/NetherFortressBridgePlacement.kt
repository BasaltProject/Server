package org.basaltmc.server.world.terrain

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.ChunkBlockEntity
import org.basaltmc.server.world.structure.StructureBoundingBox
import org.basaltmc.server.world.structure.StructureDirection
import org.basaltmc.server.world.terrain.NetherFortressPieces.AIR
import org.basaltmc.server.world.terrain.NetherFortressPieces.NETHER_BRICKS
import org.basaltmc.server.world.terrain.NetherFortressPieces.Piece
import org.basaltmc.server.world.terrain.NetherFortressPieces.SPAWNER
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtType

/** // parity: NetherFortressPieces bridge-piece block placement bodies. */
internal fun Piece.placeBridgeCrossing(
    chunk: Chunk,
    clip: StructureBoundingBox,
) {
    box(chunk, clip, 7, 3, 0, 11, 4, 18, NETHER_BRICKS)
    box(chunk, clip, 0, 3, 7, 18, 4, 11, NETHER_BRICKS)
    box(chunk, clip, 8, 5, 0, 10, 7, 18, AIR)
    box(chunk, clip, 0, 5, 8, 18, 7, 10, AIR)
    for (zRange in listOf(
        0..7,
        11..18,
    )) {
        for (x in listOf(7, 11)) box(chunk, clip, x, 5, zRange.first, x, 5, zRange.last, NETHER_BRICKS)
    }
    for (xRange in listOf(
        0..7,
        11..18,
    )) {
        for (z in listOf(7, 11)) box(chunk, clip, xRange.first, 5, z, xRange.last, 5, z, NETHER_BRICKS)
    }
    for (zRange in listOf(0..5, 13..18)) {
        box(chunk, clip, 7, 2, zRange.first, 11, 2, zRange.last, NETHER_BRICKS)
        box(
            chunk,
            clip,
            7,
            0,
            if (zRange.first == 0) 0 else 15,
            11,
            1,
            if (zRange.first == 0) 3 else zRange.last,
            NETHER_BRICKS,
        )
        for (x in 7..11) {
            for (z in 0..2) {
                fillDown(
                    chunk,
                    clip,
                    x,
                    -1,
                    if (zRange.first ==
                        0
                    ) {
                        z
                    } else {
                        18 - z
                    },
                    NETHER_BRICKS,
                )
            }
        }
    }
    for (xRange in listOf(0..5, 13..18)) {
        box(chunk, clip, xRange.first, 2, 7, xRange.last, 2, 11, NETHER_BRICKS)
        box(
            chunk,
            clip,
            if (xRange.first == 0) 0 else 15,
            0,
            7,
            if (xRange.first == 0) 3 else xRange.last,
            1,
            11,
            NETHER_BRICKS,
        )
        for (x in 0..2) {
            for (z in 7..11) {
                fillDown(
                    chunk,
                    clip,
                    if (xRange.first ==
                        0
                    ) {
                        x
                    } else {
                        18 - x
                    },
                    -1,
                    z,
                    NETHER_BRICKS,
                )
            }
        }
    }
}

internal fun Piece.placeBridgeEnd(
    chunk: Chunk,
    clip: StructureBoundingBox,
) {
    val random = WorldgenRandom(LegacyRandom(selfSeed?.toLong() ?: 0L))
    for (x in 0..4) for (y in 3..4) box(chunk, clip, x, y, 0, x, y, random.nextInt(8), NETHER_BRICKS)
    box(chunk, clip, 0, 5, 0, 0, 5, random.nextInt(8), NETHER_BRICKS)
    box(chunk, clip, 4, 5, 0, 4, 5, random.nextInt(8), NETHER_BRICKS)
    for (x in 0..4) box(chunk, clip, x, 2, 0, x, 2, random.nextInt(5), NETHER_BRICKS)
    for (x in 0..4) for (y in 0..1) box(chunk, clip, x, y, 0, x, y, random.nextInt(3), NETHER_BRICKS)
}

internal fun Piece.placeBridgeStraight(
    chunk: Chunk,
    clip: StructureBoundingBox,
) {
    box(chunk, clip, 0, 3, 0, 4, 4, 18, NETHER_BRICKS)
    box(chunk, clip, 1, 5, 0, 3, 7, 18, AIR)
    box(chunk, clip, 0, 5, 0, 0, 5, 18, NETHER_BRICKS)
    box(chunk, clip, 4, 5, 0, 4, 5, 18, NETHER_BRICKS)
    for (zRange in listOf(0..5, 13..18)) box(chunk, clip, 0, 2, zRange.first, 4, 2, zRange.last, NETHER_BRICKS)
    for (zRange in listOf(0..3, 15..18)) box(chunk, clip, 0, 0, zRange.first, 4, 1, zRange.last, NETHER_BRICKS)
    for (x in 0..4) {
        for (z in 0..2) {
            fillDown(chunk, clip, x, -1, z, NETHER_BRICKS)
            fillDown(chunk, clip, x, -1, 18 - z, NETHER_BRICKS)
        }
    }
    val nseFence = fence(StructureDirection.NORTH, StructureDirection.SOUTH, StructureDirection.EAST)
    val nswFence = fence(StructureDirection.NORTH, StructureDirection.SOUTH, StructureDirection.WEST)
    for (z in listOf(1, 4, 14, 17)) {
        val minY = if (z == 1 || z == 17) 1 else 3
        box(chunk, clip, 0, minY, z, 0, 4, z, nseFence)
        box(chunk, clip, 4, minY, z, 4, 4, z, nswFence)
    }
}

internal fun Piece.placeRoomCrossing(
    chunk: Chunk,
    clip: StructureBoundingBox,
) {
    box(chunk, clip, 0, 0, 0, 6, 1, 6, NETHER_BRICKS)
    box(chunk, clip, 0, 2, 0, 6, 7, 6, AIR)
    box(chunk, clip, 0, 2, 0, 1, 6, 0, NETHER_BRICKS)
    box(chunk, clip, 0, 2, 6, 1, 6, 6, NETHER_BRICKS)
    box(chunk, clip, 5, 2, 0, 6, 6, 0, NETHER_BRICKS)
    box(chunk, clip, 5, 2, 6, 6, 6, 6, NETHER_BRICKS)
    box(chunk, clip, 0, 2, 0, 0, 6, 1, NETHER_BRICKS)
    box(chunk, clip, 0, 2, 5, 0, 6, 6, NETHER_BRICKS)
    box(chunk, clip, 6, 2, 0, 6, 6, 1, NETHER_BRICKS)
    box(chunk, clip, 6, 2, 5, 6, 6, 6, NETHER_BRICKS)
    val weFence = fence(StructureDirection.WEST, StructureDirection.EAST)
    val nsFence = fence(StructureDirection.NORTH, StructureDirection.SOUTH)
    box(chunk, clip, 2, 6, 0, 4, 6, 0, NETHER_BRICKS)
    box(chunk, clip, 2, 5, 0, 4, 5, 0, weFence)
    box(chunk, clip, 2, 6, 6, 4, 6, 6, NETHER_BRICKS)
    box(chunk, clip, 2, 5, 6, 4, 5, 6, weFence)
    box(chunk, clip, 0, 6, 2, 0, 6, 4, NETHER_BRICKS)
    box(chunk, clip, 0, 5, 2, 0, 5, 4, nsFence)
    box(chunk, clip, 6, 6, 2, 6, 6, 4, NETHER_BRICKS)
    box(chunk, clip, 6, 5, 2, 6, 5, 4, nsFence)
    for (x in 0..6) for (z in 0..6) fillDown(chunk, clip, x, -1, z, NETHER_BRICKS)
}

internal fun Piece.placeStairsRoom(
    chunk: Chunk,
    clip: StructureBoundingBox,
) {
    box(chunk, clip, 0, 0, 0, 6, 1, 6, NETHER_BRICKS)
    box(chunk, clip, 0, 2, 0, 6, 10, 6, AIR)
    box(chunk, clip, 0, 2, 0, 1, 8, 0, NETHER_BRICKS)
    box(chunk, clip, 5, 2, 0, 6, 8, 0, NETHER_BRICKS)
    box(chunk, clip, 0, 2, 1, 0, 8, 6, NETHER_BRICKS)
    box(chunk, clip, 6, 2, 1, 6, 8, 6, NETHER_BRICKS)
    box(chunk, clip, 1, 2, 6, 5, 8, 6, NETHER_BRICKS)
    val weFence = fence(StructureDirection.WEST, StructureDirection.EAST)
    val nsFence = fence(StructureDirection.NORTH, StructureDirection.SOUTH)
    box(chunk, clip, 0, 3, 2, 0, 5, 4, nsFence)
    box(chunk, clip, 6, 3, 2, 6, 5, 2, nsFence)
    box(chunk, clip, 6, 3, 4, 6, 5, 4, nsFence)
    placeBlock(chunk, clip, 5, 2, 5, NETHER_BRICKS)
    box(chunk, clip, 4, 2, 5, 4, 3, 5, NETHER_BRICKS)
    box(chunk, clip, 3, 2, 5, 3, 4, 5, NETHER_BRICKS)
    box(chunk, clip, 2, 2, 5, 2, 5, 5, NETHER_BRICKS)
    box(chunk, clip, 1, 2, 5, 1, 6, 5, NETHER_BRICKS)
    box(chunk, clip, 1, 7, 1, 5, 7, 4, NETHER_BRICKS)
    box(chunk, clip, 6, 8, 2, 6, 8, 4, AIR)
    box(chunk, clip, 2, 6, 0, 4, 8, 0, NETHER_BRICKS)
    box(chunk, clip, 2, 5, 0, 4, 5, 0, weFence)
    for (x in 0..6) for (z in 0..6) fillDown(chunk, clip, x, -1, z, NETHER_BRICKS)
}

internal fun Piece.placeMonsterThrone(
    chunk: Chunk,
    clip: StructureBoundingBox,
) {
    box(chunk, clip, 0, 2, 0, 6, 7, 7, AIR)
    box(chunk, clip, 1, 0, 0, 5, 1, 7, NETHER_BRICKS)
    box(chunk, clip, 1, 2, 1, 5, 2, 7, NETHER_BRICKS)
    box(chunk, clip, 1, 3, 2, 5, 3, 7, NETHER_BRICKS)
    box(chunk, clip, 1, 4, 3, 5, 4, 7, NETHER_BRICKS)
    box(chunk, clip, 1, 2, 0, 1, 4, 2, NETHER_BRICKS)
    box(chunk, clip, 5, 2, 0, 5, 4, 2, NETHER_BRICKS)
    box(chunk, clip, 1, 5, 2, 1, 5, 3, NETHER_BRICKS)
    box(chunk, clip, 5, 5, 2, 5, 5, 3, NETHER_BRICKS)
    box(chunk, clip, 0, 5, 3, 0, 5, 8, NETHER_BRICKS)
    box(chunk, clip, 6, 5, 3, 6, 5, 8, NETHER_BRICKS)
    box(chunk, clip, 1, 5, 8, 5, 5, 8, NETHER_BRICKS)
    val weFence = fence(StructureDirection.WEST, StructureDirection.EAST)
    val nsFence = fence(StructureDirection.NORTH, StructureDirection.SOUTH)
    placeBlock(chunk, clip, 1, 6, 3, fence(StructureDirection.WEST))
    placeBlock(chunk, clip, 5, 6, 3, fence(StructureDirection.EAST))
    placeBlock(chunk, clip, 0, 6, 3, fence(StructureDirection.EAST, StructureDirection.NORTH))
    placeBlock(chunk, clip, 6, 6, 3, fence(StructureDirection.WEST, StructureDirection.NORTH))
    box(chunk, clip, 0, 6, 4, 0, 6, 7, nsFence)
    box(chunk, clip, 6, 6, 4, 6, 6, 7, nsFence)
    placeBlock(chunk, clip, 0, 6, 8, fence(StructureDirection.EAST, StructureDirection.SOUTH))
    placeBlock(chunk, clip, 6, 6, 8, fence(StructureDirection.WEST, StructureDirection.SOUTH))
    box(chunk, clip, 1, 6, 8, 5, 6, 8, weFence)
    placeBlock(chunk, clip, 1, 7, 8, fence(StructureDirection.EAST))
    box(chunk, clip, 2, 7, 8, 4, 7, 8, weFence)
    placeBlock(chunk, clip, 5, 7, 8, fence(StructureDirection.WEST))
    placeBlock(chunk, clip, 2, 8, 8, fence(StructureDirection.EAST))
    placeBlock(chunk, clip, 3, 8, 8, weFence)
    placeBlock(chunk, clip, 4, 8, 8, fence(StructureDirection.WEST))
    placeSpawner(chunk, clip, 3, 5, 5)
    for (x in 0..6) for (z in 0..6) fillDown(chunk, clip, x, -1, z, NETHER_BRICKS)
}

internal fun Piece.placeSpawner(
    chunk: Chunk,
    clip: StructureBoundingBox,
    x: Int,
    y: Int,
    z: Int,
) {
    if (!isInside(x, y, z, clip)) return
    val worldX = worldX(x, z)
    val worldY = boundingBox.minY + y
    val worldZ = worldZ(x, z)
    chunk.setBlockStateId(worldX and 15, worldY, worldZ and 15, SPAWNER)
    val spawnData =
        NbtCompound().apply {
            putCompound("entity", NbtCompound().apply { putString("id", "minecraft:blaze") })
        }
    val data =
        NbtCompound().apply {
            putString("id", "minecraft:mob_spawner")
            putShort("Delay", 20)
            putShort("MinSpawnDelay", 200)
            putShort("MaxSpawnDelay", 800)
            putShort("SpawnCount", 4)
            putShort("MaxNearbyEntities", 6)
            putShort("RequiredPlayerRange", 16)
            putShort("SpawnRange", 4)
            putCompound("SpawnData", spawnData)
            this["SpawnPotentials"] = NbtList(NbtType.END)
            putCompound("components", NbtCompound())
        }
    chunk.setBlockEntity(
        ChunkBlockEntity(worldX and 15, worldY, worldZ and 15, Identifier.vanilla("mob_spawner"), data),
    )
}
