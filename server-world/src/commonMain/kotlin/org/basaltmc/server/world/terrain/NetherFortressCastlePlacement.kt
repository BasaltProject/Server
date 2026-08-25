package org.basaltmc.server.world.terrain

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.ChunkBlockEntity
import org.basaltmc.server.world.ChunkScheduledTick
import org.basaltmc.server.world.structure.StructureBoundingBox
import org.basaltmc.server.world.structure.StructureDirection
import org.basaltmc.server.world.terrain.NetherFortressPieces.AIR
import org.basaltmc.server.world.terrain.NetherFortressPieces.LAVA
import org.basaltmc.server.world.terrain.NetherFortressPieces.NETHER_BRICKS
import org.basaltmc.server.world.terrain.NetherFortressPieces.NETHER_BRICK_FENCE
import org.basaltmc.server.world.terrain.NetherFortressPieces.NETHER_WART
import org.basaltmc.server.world.terrain.NetherFortressPieces.Piece
import org.basaltmc.server.world.terrain.NetherFortressPieces.PieceType
import org.basaltmc.server.world.terrain.NetherFortressPieces.SOUL_SAND
import org.basaltmc.server.world.terrain.NetherFortressPieces.stateId
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound

/** // parity: NetherFortressPieces castle-piece block placement bodies. */
internal fun Piece.placeCastleEntrance(
    chunk: Chunk,
    clip: StructureBoundingBox,
) {
    placeCastleOuterShell(chunk, clip, frontGate = true)
    placeCastleFoundation(chunk, clip)
    box(chunk, clip, 5, 5, 5, 7, 5, 7, NETHER_BRICKS)
    box(chunk, clip, 6, 1, 6, 6, 4, 6, AIR)
    placeBlock(chunk, clip, 6, 0, 6, NETHER_BRICKS)
    placeBlock(chunk, clip, 6, 5, 6, LAVA)
    if (isInside(6, 5, 6, clip)) {
        chunk.scheduleFluidTick(
            ChunkScheduledTick(
                Identifier.vanilla("lava"),
                worldX(6, 6) and 15,
                boundingBox.minY + 5,
                worldZ(6, 6) and 15,
                delay = 0,
            ),
        )
    }
}

internal fun Piece.placeCastleSmallCorridor(
    chunk: Chunk,
    clip: StructureBoundingBox,
) {
    box(chunk, clip, 0, 0, 0, 4, 1, 4, NETHER_BRICKS)
    box(chunk, clip, 0, 2, 0, 4, 5, 4, AIR)
    box(chunk, clip, 0, 2, 0, 0, 5, 4, NETHER_BRICKS)
    box(chunk, clip, 4, 2, 0, 4, 5, 4, NETHER_BRICKS)
    val nsFence = fence(StructureDirection.NORTH, StructureDirection.SOUTH)
    for (x in listOf(0, 4)) for (z in listOf(1, 3)) box(chunk, clip, x, 3, z, x, 4, z, nsFence)
    box(chunk, clip, 0, 6, 0, 4, 6, 4, NETHER_BRICKS)
    for (x in 0..4) for (z in 0..4) fillDown(chunk, clip, x, -1, z, NETHER_BRICKS)
}

internal fun Piece.placeCastleSmallCrossing(
    chunk: Chunk,
    clip: StructureBoundingBox,
) {
    box(chunk, clip, 0, 0, 0, 4, 1, 4, NETHER_BRICKS)
    box(chunk, clip, 0, 2, 0, 4, 5, 4, AIR)
    box(chunk, clip, 0, 2, 0, 0, 5, 0, NETHER_BRICKS)
    box(chunk, clip, 4, 2, 0, 4, 5, 0, NETHER_BRICKS)
    box(chunk, clip, 0, 2, 4, 0, 5, 4, NETHER_BRICKS)
    box(chunk, clip, 4, 2, 4, 4, 5, 4, NETHER_BRICKS)
    box(chunk, clip, 0, 6, 0, 4, 6, 4, NETHER_BRICKS)
    for (x in 0..4) for (z in 0..4) fillDown(chunk, clip, x, -1, z, NETHER_BRICKS)
}

internal fun Piece.placeCastleLeftTurn(
    chunk: Chunk,
    clip: StructureBoundingBox,
    random: WorldgenRandom,
) {
    placeCastleTurn(chunk, clip, wallOnLeft = false)
    if (needsChest && isInside(3, 2, 3, clip) && !isChestAt(chunk, 3, 2, 3)) {
        placeFortressChest(chunk, 3, 2, 3, random)
    }
}

internal fun Piece.placeCastleRightTurn(
    chunk: Chunk,
    clip: StructureBoundingBox,
    random: WorldgenRandom,
) {
    placeCastleTurn(chunk, clip, wallOnLeft = true)
    if (needsChest && isInside(1, 2, 3, clip) && !isChestAt(chunk, 1, 2, 3)) {
        placeFortressChest(chunk, 1, 2, 3, random)
    }
}

internal fun Piece.placeFortressChest(
    chunk: Chunk,
    x: Int,
    y: Int,
    z: Int,
    random: WorldgenRandom,
) {
    val facesSouth =
        (type == PieceType.CASTLE_LEFT_TURN && orientation == StructureDirection.NORTH) ||
            (type == PieceType.CASTLE_RIGHT_TURN && orientation != StructureDirection.SOUTH)
    val facing = if (facesSouth) StructureDirection.SOUTH else StructureDirection.NORTH
    val stateId =
        stateId(
            "chest",
            linkedMapOf(
                "facing" to facing.name.lowercase(),
                "type" to "single",
                "waterlogged" to "false",
            ),
        )
    val worldX = worldX(x, z)
    val worldY = boundingBox.minY + y
    val worldZ = worldZ(x, z)
    chunk.setBlockStateId(worldX and 15, worldY, worldZ and 15, stateId)
    val data =
        NbtCompound().apply {
            putString("id", "minecraft:chest")
            putString("LootTable", "minecraft:chests/nether_bridge")
            putLong("LootTableSeed", random.nextLong())
            putCompound("components", NbtCompound())
        }
    chunk.setBlockEntity(
        ChunkBlockEntity(worldX and 15, worldY, worldZ and 15, Identifier.vanilla("chest"), data),
    )
}

internal fun Piece.placeCastleTurn(
    chunk: Chunk,
    clip: StructureBoundingBox,
    wallOnLeft: Boolean,
) {
    box(chunk, clip, 0, 0, 0, 4, 1, 4, NETHER_BRICKS)
    box(chunk, clip, 0, 2, 0, 4, 5, 4, AIR)
    val wallX = if (wallOnLeft) 0 else 4
    box(chunk, clip, wallX, 2, 0, wallX, 5, 4, NETHER_BRICKS)
    val nsFence = fence(StructureDirection.NORTH, StructureDirection.SOUTH)
    val weFence = fence(StructureDirection.WEST, StructureDirection.EAST)
    for (z in listOf(1, 3)) box(chunk, clip, wallX, 3, z, wallX, 4, z, nsFence)
    if (wallOnLeft) {
        box(chunk, clip, 4, 2, 0, 4, 5, 0, NETHER_BRICKS)
        box(chunk, clip, 1, 2, 4, 4, 5, 4, NETHER_BRICKS)
    } else {
        box(chunk, clip, 0, 2, 0, 0, 5, 0, NETHER_BRICKS)
        box(chunk, clip, 0, 2, 4, 3, 5, 4, NETHER_BRICKS)
    }
    for (x in listOf(1, 3)) box(chunk, clip, x, 3, 4, x, 4, 4, weFence)
    box(chunk, clip, 0, 6, 0, 4, 6, 4, NETHER_BRICKS)
    for (x in 0..4) for (z in 0..4) fillDown(chunk, clip, x, -1, z, NETHER_BRICKS)
}

internal fun Piece.placeCastleStairs(
    chunk: Chunk,
    clip: StructureBoundingBox,
) {
    for (step in 0..9) {
        val floor = maxOf(1, 7 - step)
        val roof = minOf(maxOf(floor + 5, 14 - step), 13)
        box(chunk, clip, 0, 0, step, 4, floor, step, NETHER_BRICKS)
        box(chunk, clip, 1, floor + 1, step, 3, roof - 1, step, AIR)
        if (step <=
            6
        ) {
            box(chunk, clip, 1, floor + 1, step, 3, floor + 1, step, stairs(StructureDirection.SOUTH))
        }
        box(chunk, clip, 0, roof, step, 4, roof, step, NETHER_BRICKS)
        box(chunk, clip, 0, floor + 1, step, 0, roof - 1, step, NETHER_BRICKS)
        box(chunk, clip, 4, floor + 1, step, 4, roof - 1, step, NETHER_BRICKS)
        if (step and 1 == 0) {
            val nsFence = fence(StructureDirection.NORTH, StructureDirection.SOUTH)
            box(chunk, clip, 0, floor + 2, step, 0, floor + 3, step, nsFence)
            box(chunk, clip, 4, floor + 2, step, 4, floor + 3, step, nsFence)
        }
        for (x in 0..4) fillDown(chunk, clip, x, -1, step, NETHER_BRICKS)
    }
}

internal fun Piece.placeCastleTBalcony(
    chunk: Chunk,
    clip: StructureBoundingBox,
) {
    val nsFence = fence(StructureDirection.NORTH, StructureDirection.SOUTH)
    val weFence = fence(StructureDirection.WEST, StructureDirection.EAST)
    box(chunk, clip, 0, 0, 0, 8, 1, 8, NETHER_BRICKS)
    box(chunk, clip, 0, 2, 0, 8, 5, 8, AIR)
    box(chunk, clip, 0, 6, 0, 8, 6, 5, NETHER_BRICKS)
    box(chunk, clip, 0, 2, 0, 2, 5, 0, NETHER_BRICKS)
    box(chunk, clip, 6, 2, 0, 8, 5, 0, NETHER_BRICKS)
    box(chunk, clip, 1, 3, 0, 1, 4, 0, weFence)
    box(chunk, clip, 7, 3, 0, 7, 4, 0, weFence)
    box(chunk, clip, 0, 2, 4, 8, 2, 8, NETHER_BRICKS)
    box(chunk, clip, 1, 1, 4, 2, 2, 4, AIR)
    box(chunk, clip, 6, 1, 4, 7, 2, 4, AIR)
    box(chunk, clip, 1, 3, 8, 7, 3, 8, weFence)
    placeBlock(chunk, clip, 0, 3, 8, fence(StructureDirection.EAST, StructureDirection.SOUTH))
    placeBlock(chunk, clip, 8, 3, 8, fence(StructureDirection.WEST, StructureDirection.SOUTH))
    box(chunk, clip, 0, 3, 6, 0, 3, 7, nsFence)
    box(chunk, clip, 8, 3, 6, 8, 3, 7, nsFence)
    box(chunk, clip, 0, 3, 4, 0, 5, 5, NETHER_BRICKS)
    box(chunk, clip, 8, 3, 4, 8, 5, 5, NETHER_BRICKS)
    box(chunk, clip, 1, 3, 5, 2, 5, 5, NETHER_BRICKS)
    box(chunk, clip, 6, 3, 5, 7, 5, 5, NETHER_BRICKS)
    box(chunk, clip, 1, 4, 5, 1, 5, 5, weFence)
    box(chunk, clip, 7, 4, 5, 7, 5, 5, weFence)
    for (z in 0..5) for (x in 0..8) fillDown(chunk, clip, x, -1, z, NETHER_BRICKS)
}

internal fun Piece.placeCastleStalkRoom(
    chunk: Chunk,
    clip: StructureBoundingBox,
) {
    placeCastleOuterShell(chunk, clip, frontGate = false)
    val northStairs = stairs(StructureDirection.NORTH)
    val nseFence = fence(StructureDirection.NORTH, StructureDirection.SOUTH, StructureDirection.EAST)
    val nswFence = fence(StructureDirection.NORTH, StructureDirection.SOUTH, StructureDirection.WEST)
    for (i in 0..6) {
        val z = i + 4
        box(chunk, clip, 5, 5 + i, z, 7, 5 + i, z, northStairs)
        if (z in 5..8) {
            box(chunk, clip, 5, 5, z, 7, i + 4, z, NETHER_BRICKS)
        } else if (z in 9..10) {
            box(chunk, clip, 5, 8, z, 7, i + 4, z, NETHER_BRICKS)
        }
        if (i >= 1) box(chunk, clip, 5, 6 + i, z, 7, 9 + i, z, AIR)
    }
    box(chunk, clip, 5, 12, 11, 7, 12, 11, northStairs)
    box(chunk, clip, 5, 6, 7, 5, 7, 7, nseFence)
    box(chunk, clip, 7, 6, 7, 7, 7, 7, nswFence)
    box(chunk, clip, 5, 13, 12, 7, 13, 12, AIR)
    box(chunk, clip, 2, 5, 2, 3, 5, 3, NETHER_BRICKS)
    box(chunk, clip, 2, 5, 9, 3, 5, 10, NETHER_BRICKS)
    box(chunk, clip, 2, 5, 4, 2, 5, 8, NETHER_BRICKS)
    box(chunk, clip, 9, 5, 2, 10, 5, 3, NETHER_BRICKS)
    box(chunk, clip, 9, 5, 9, 10, 5, 10, NETHER_BRICKS)
    box(chunk, clip, 10, 5, 4, 10, 5, 8, NETHER_BRICKS)
    val westStairs = stairs(StructureDirection.WEST)
    val eastStairs = stairs(StructureDirection.EAST)
    for (z in listOf(2, 3, 9, 10)) {
        placeBlock(chunk, clip, 4, 5, z, westStairs)
        placeBlock(chunk, clip, 8, 5, z, eastStairs)
    }
    box(chunk, clip, 3, 4, 4, 4, 4, 8, SOUL_SAND)
    box(chunk, clip, 8, 4, 4, 9, 4, 8, SOUL_SAND)
    box(chunk, clip, 3, 5, 4, 4, 5, 8, NETHER_WART)
    box(chunk, clip, 8, 5, 4, 9, 5, 8, NETHER_WART)
    placeCastleFoundation(chunk, clip)
}

internal fun Piece.placeCastleOuterShell(
    chunk: Chunk,
    clip: StructureBoundingBox,
    frontGate: Boolean,
) {
    box(chunk, clip, 0, 3, 0, 12, 4, 12, NETHER_BRICKS)
    box(chunk, clip, 0, 5, 0, 12, 13, 12, AIR)
    box(chunk, clip, 0, 5, 0, 1, 12, 12, NETHER_BRICKS)
    box(chunk, clip, 11, 5, 0, 12, 12, 12, NETHER_BRICKS)
    box(chunk, clip, 2, 5, 11, 4, 12, 12, NETHER_BRICKS)
    box(chunk, clip, 8, 5, 11, 10, 12, 12, NETHER_BRICKS)
    box(chunk, clip, 5, 9, 11, 7, 12, 12, NETHER_BRICKS)
    box(chunk, clip, 2, 5, 0, 4, 12, 1, NETHER_BRICKS)
    box(chunk, clip, 8, 5, 0, 10, 12, 1, NETHER_BRICKS)
    box(chunk, clip, 5, 9, 0, 7, 12, 1, NETHER_BRICKS)
    box(chunk, clip, 2, 11, 2, 10, 12, 10, NETHER_BRICKS)
    if (frontGate) box(chunk, clip, 5, 8, 0, 7, 8, 0, NETHER_BRICK_FENCE)
    val weFence = fence(StructureDirection.WEST, StructureDirection.EAST)
    val nsFence = fence(StructureDirection.NORTH, StructureDirection.SOUTH)
    for (i in 1..11 step 2) {
        box(chunk, clip, i, 10, 0, i, 11, 0, weFence)
        box(chunk, clip, i, 10, 12, i, 11, 12, weFence)
        box(chunk, clip, 0, 10, i, 0, 11, i, nsFence)
        box(chunk, clip, 12, 10, i, 12, 11, i, nsFence)
        placeBlock(chunk, clip, i, 13, 0, NETHER_BRICKS)
        placeBlock(chunk, clip, i, 13, 12, NETHER_BRICKS)
        placeBlock(chunk, clip, 0, 13, i, NETHER_BRICKS)
        placeBlock(chunk, clip, 12, 13, i, NETHER_BRICKS)
        if (i != 11) {
            placeBlock(chunk, clip, i + 1, 13, 0, weFence)
            placeBlock(chunk, clip, i + 1, 13, 12, weFence)
            placeBlock(chunk, clip, 0, 13, i + 1, nsFence)
            placeBlock(chunk, clip, 12, 13, i + 1, nsFence)
        }
    }
    placeBlock(chunk, clip, 0, 13, 0, fence(StructureDirection.NORTH, StructureDirection.EAST))
    placeBlock(chunk, clip, 0, 13, 12, fence(StructureDirection.SOUTH, StructureDirection.EAST))
    placeBlock(chunk, clip, 12, 13, 12, fence(StructureDirection.SOUTH, StructureDirection.WEST))
    placeBlock(chunk, clip, 12, 13, 0, fence(StructureDirection.NORTH, StructureDirection.WEST))
    for (z in 3..9 step 2) {
        box(
            chunk,
            clip,
            1,
            7,
            z,
            1,
            8,
            z,
            fence(StructureDirection.NORTH, StructureDirection.SOUTH, StructureDirection.WEST),
        )
        box(
            chunk,
            clip,
            11,
            7,
            z,
            11,
            8,
            z,
            fence(StructureDirection.NORTH, StructureDirection.SOUTH, StructureDirection.EAST),
        )
    }
}

internal fun Piece.placeCastleFoundation(
    chunk: Chunk,
    clip: StructureBoundingBox,
) {
    box(chunk, clip, 4, 2, 0, 8, 2, 12, NETHER_BRICKS)
    box(chunk, clip, 0, 2, 4, 12, 2, 8, NETHER_BRICKS)
    box(chunk, clip, 4, 0, 0, 8, 1, 3, NETHER_BRICKS)
    box(chunk, clip, 4, 0, 9, 8, 1, 12, NETHER_BRICKS)
    box(chunk, clip, 0, 0, 4, 3, 1, 8, NETHER_BRICKS)
    box(chunk, clip, 9, 0, 4, 12, 1, 8, NETHER_BRICKS)
    for (x in 4..8) {
        for (z in 0..2) {
            fillDown(chunk, clip, x, -1, z, NETHER_BRICKS)
            fillDown(chunk, clip, x, -1, 12 - z, NETHER_BRICKS)
        }
    }
    for (x in 0..2) {
        for (z in 4..8) {
            fillDown(chunk, clip, x, -1, z, NETHER_BRICKS)
            fillDown(chunk, clip, 12 - x, -1, z, NETHER_BRICKS)
        }
    }
}
