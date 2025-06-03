package org.basaltmc.world.terrain

import org.basaltmc.common.Identifier
import org.basaltmc.nbt.NbtCompound
import org.basaltmc.nbt.NbtList
import org.basaltmc.nbt.NbtType
import org.basaltmc.random.LegacyRandom
import org.basaltmc.random.WorldgenRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.world.Chunk
import org.basaltmc.world.ChunkBlockEntity
import org.basaltmc.world.ChunkScheduledTick
import org.basaltmc.world.structure.StructureBoundingBox
import org.basaltmc.world.structure.StructureDirection

/** // parity: NetherFortressStructure.generatePieces and NetherFortressPieces layout assembly. */
internal object NetherFortressPieces {
    fun assemble(
        random: WorldgenRandom,
        chunkX: Int,
        chunkZ: Int,
    ): List<Piece> {
        val direction = HORIZONTAL_DIRECTIONS[random.nextInt(HORIZONTAL_DIRECTIONS.size)]
        val startBox = makeBoundingBox(chunkX * 16 + 2, 64, chunkZ * 16 + 2, direction, 19, 10, 19)
        val state = AssemblyState(Piece(PieceType.START, 0, startBox, direction))
        state.pieces += state.start
        state.start.addChildren(state, random)
        while (state.pending.isNotEmpty()) {
            val piece = state.pending.removeAt(random.nextInt(state.pending.size))
            piece.addChildren(state, random)
        }
        val bounds = state.pieces.boundingBox()
        val heightSpan = 70 - 48 + 1 - (bounds.maxY - bounds.minY + 1)
        val targetMinY = if (heightSpan > 1) 48 + random.nextInt(heightSpan) else 48
        val dy = targetMinY - bounds.minY
        state.pieces.forEach { it.move(0, dy, 0) }
        return state.pieces
    }

    internal class Piece(
        internal val type: PieceType,
        internal val generationDepth: Int,
        internal var boundingBox: StructureBoundingBox,
        internal val orientation: StructureDirection,
        internal val selfSeed: Int? = null,
        internal val needsChest: Boolean = false,
    ) {
        fun place(
            chunk: Chunk,
            clip: StructureBoundingBox,
            placementRandom: WorldgenRandom,
        ) {
            when (type) {
                PieceType.START, PieceType.BRIDGE_CROSSING -> placeBridgeCrossing(chunk, clip)
                PieceType.BRIDGE_STRAIGHT -> placeBridgeStraight(chunk, clip)
                PieceType.BRIDGE_END_FILLER -> placeBridgeEnd(chunk, clip)
                PieceType.ROOM_CROSSING -> placeRoomCrossing(chunk, clip)
                PieceType.STAIRS_ROOM -> placeStairsRoom(chunk, clip)
                PieceType.MONSTER_THRONE -> placeMonsterThrone(chunk, clip)
                PieceType.CASTLE_ENTRANCE -> placeCastleEntrance(chunk, clip)
                PieceType.CASTLE_SMALL_CORRIDOR -> placeCastleSmallCorridor(chunk, clip)
                PieceType.CASTLE_SMALL_CROSSING -> placeCastleSmallCrossing(chunk, clip)
                PieceType.CASTLE_LEFT_TURN -> placeCastleLeftTurn(chunk, clip, placementRandom)
                PieceType.CASTLE_RIGHT_TURN -> placeCastleRightTurn(chunk, clip, placementRandom)
                PieceType.CASTLE_STAIRS -> placeCastleStairs(chunk, clip)
                PieceType.CASTLE_T_BALCONY -> placeCastleTBalcony(chunk, clip)
                PieceType.CASTLE_STALK_ROOM -> placeCastleStalkRoom(chunk, clip)
            }
        }

        fun move(
            dx: Int,
            dy: Int,
            dz: Int,
        ) {
            boundingBox =
                boundingBox.move(
                    org.basaltmc.world.structure
                        .StructurePos(dx, dy, dz),
                )
        }

        internal fun addChildren(
            state: AssemblyState,
            random: WorldgenRandom,
        ) {
            when (type) {
                PieceType.START, PieceType.BRIDGE_CROSSING -> {
                    childForward(state, random, 8, 3, false)
                    childLeft(state, random, 3, 8, false)
                    childRight(state, random, 3, 8, false)
                }
                PieceType.BRIDGE_STRAIGHT -> childForward(state, random, 1, 3, false)
                PieceType.ROOM_CROSSING -> {
                    childForward(state, random, 2, 0, false)
                    childLeft(state, random, 0, 2, false)
                    childRight(state, random, 0, 2, false)
                }
                PieceType.STAIRS_ROOM -> childRight(state, random, 6, 2, false)
                PieceType.CASTLE_ENTRANCE -> childForward(state, random, 5, 3, true)
                PieceType.CASTLE_SMALL_CORRIDOR -> childForward(state, random, 1, 0, true)
                PieceType.CASTLE_SMALL_CROSSING -> {
                    childForward(state, random, 1, 0, true)
                    childLeft(state, random, 0, 1, true)
                    childRight(state, random, 0, 1, true)
                }
                PieceType.CASTLE_LEFT_TURN -> childLeft(state, random, 0, 1, true)
                PieceType.CASTLE_RIGHT_TURN -> childRight(state, random, 0, 1, true)
                PieceType.CASTLE_STAIRS -> childForward(state, random, 1, 0, true)
                PieceType.CASTLE_T_BALCONY -> {
                    val zOffset =
                        if (orientation == StructureDirection.WEST ||
                            orientation == StructureDirection.NORTH
                        ) {
                            5
                        } else {
                            1
                        }
                    childLeft(state, random, 0, zOffset, random.nextInt(8) > 0)
                    childRight(state, random, 0, zOffset, random.nextInt(8) > 0)
                }
                PieceType.CASTLE_STALK_ROOM -> {
                    childForward(state, random, 5, 3, true)
                    childForward(state, random, 5, 11, true)
                }
                PieceType.BRIDGE_END_FILLER, PieceType.MONSTER_THRONE -> Unit
            }
        }

        private fun childForward(
            state: AssemblyState,
            random: WorldgenRandom,
            xOffset: Int,
            yOffset: Int,
            castle: Boolean,
        ) {
            when (orientation) {
                StructureDirection.NORTH ->
                    addPiece(
                        state,
                        random,
                        boundingBox.minX + xOffset,
                        boundingBox.minY + yOffset,
                        boundingBox.minZ - 1,
                        orientation,
                        castle,
                    )
                StructureDirection.SOUTH ->
                    addPiece(
                        state,
                        random,
                        boundingBox.minX + xOffset,
                        boundingBox.minY + yOffset,
                        boundingBox.maxZ + 1,
                        orientation,
                        castle,
                    )
                StructureDirection.WEST ->
                    addPiece(
                        state,
                        random,
                        boundingBox.minX - 1,
                        boundingBox.minY + yOffset,
                        boundingBox.minZ + xOffset,
                        orientation,
                        castle,
                    )
                StructureDirection.EAST ->
                    addPiece(
                        state,
                        random,
                        boundingBox.maxX + 1,
                        boundingBox.minY + yOffset,
                        boundingBox.minZ + xOffset,
                        orientation,
                        castle,
                    )
                else -> Unit
            }
        }

        private fun childLeft(
            state: AssemblyState,
            random: WorldgenRandom,
            yOffset: Int,
            zOffset: Int,
            castle: Boolean,
        ) {
            when (orientation) {
                StructureDirection.NORTH, StructureDirection.SOUTH ->
                    addPiece(
                        state,
                        random,
                        boundingBox.minX - 1,
                        boundingBox.minY + yOffset,
                        boundingBox.minZ + zOffset,
                        StructureDirection.WEST,
                        castle,
                    )
                StructureDirection.WEST, StructureDirection.EAST ->
                    addPiece(
                        state,
                        random,
                        boundingBox.minX + zOffset,
                        boundingBox.minY + yOffset,
                        boundingBox.minZ - 1,
                        StructureDirection.NORTH,
                        castle,
                    )
                else -> Unit
            }
        }

        private fun childRight(
            state: AssemblyState,
            random: WorldgenRandom,
            yOffset: Int,
            zOffset: Int,
            castle: Boolean,
        ) {
            when (orientation) {
                StructureDirection.NORTH, StructureDirection.SOUTH ->
                    addPiece(
                        state,
                        random,
                        boundingBox.maxX + 1,
                        boundingBox.minY + yOffset,
                        boundingBox.minZ + zOffset,
                        StructureDirection.EAST,
                        castle,
                    )
                StructureDirection.WEST, StructureDirection.EAST ->
                    addPiece(
                        state,
                        random,
                        boundingBox.minX + zOffset,
                        boundingBox.minY + yOffset,
                        boundingBox.maxZ + 1,
                        StructureDirection.SOUTH,
                        castle,
                    )
                else -> Unit
            }
        }

        private fun addPiece(
            state: AssemblyState,
            random: WorldgenRandom,
            x: Int,
            y: Int,
            z: Int,
            direction: StructureDirection,
            castle: Boolean,
        ) {
            if (kotlin.math.abs(x - state.start.boundingBox.minX) > 112 ||
                kotlin.math.abs(z - state.start.boundingBox.minZ) > 112
            ) {
                createPiece(PieceType.BRIDGE_END_FILLER, state, random, x, y, z, direction, generationDepth)
                return
            }
            val available = if (castle) state.castleWeights else state.bridgeWeights
            val piece = generatePiece(state, available, random, x, y, z, direction, generationDepth + 1) ?: return
            state.pieces += piece
            state.pending += piece
        }

        private fun placeBridgeCrossing(
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

        private fun placeBridgeEnd(
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

        private fun placeBridgeStraight(
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

        private fun placeRoomCrossing(
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

        private fun placeStairsRoom(
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

        private fun placeMonsterThrone(
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

        private fun placeCastleEntrance(
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

        private fun placeCastleSmallCorridor(
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

        private fun placeCastleSmallCrossing(
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

        private fun placeCastleLeftTurn(
            chunk: Chunk,
            clip: StructureBoundingBox,
            random: WorldgenRandom,
        ) {
            placeCastleTurn(chunk, clip, wallOnLeft = false)
            if (needsChest && isInside(3, 2, 3, clip) && !isChestAt(chunk, 3, 2, 3)) {
                placeFortressChest(chunk, 3, 2, 3, random)
            }
        }

        private fun placeCastleRightTurn(
            chunk: Chunk,
            clip: StructureBoundingBox,
            random: WorldgenRandom,
        ) {
            placeCastleTurn(chunk, clip, wallOnLeft = true)
            if (needsChest && isInside(1, 2, 3, clip) && !isChestAt(chunk, 1, 2, 3)) {
                placeFortressChest(chunk, 1, 2, 3, random)
            }
        }

        private fun placeFortressChest(
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

        private fun placeSpawner(
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

        private fun placeCastleTurn(
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

        private fun placeCastleStairs(
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

        private fun placeCastleTBalcony(
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

        private fun placeCastleStalkRoom(
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

        private fun placeCastleOuterShell(
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

        private fun placeCastleFoundation(
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

        private fun fence(vararg connections: StructureDirection): Int {
            val transformed = connections.map(::transformDirection).toSet()
            return stateId(
                "nether_brick_fence",
                linkedMapOf(
                    "east" to (StructureDirection.EAST in transformed).toString(),
                    "north" to (StructureDirection.NORTH in transformed).toString(),
                    "south" to (StructureDirection.SOUTH in transformed).toString(),
                    "waterlogged" to "false",
                    "west" to (StructureDirection.WEST in transformed).toString(),
                ),
            )
        }

        private fun stairs(facing: StructureDirection): Int =
            stateId(
                "nether_brick_stairs",
                linkedMapOf(
                    "facing" to transformDirection(facing).name.lowercase(),
                    "half" to "bottom",
                    "shape" to "straight",
                    "waterlogged" to "false",
                ),
            )

        private fun transformDirection(direction: StructureDirection): StructureDirection {
            var transformed = direction
            if (orientation == StructureDirection.SOUTH || orientation == StructureDirection.WEST) {
                transformed =
                    when (transformed) {
                        StructureDirection.NORTH -> StructureDirection.SOUTH
                        StructureDirection.SOUTH -> StructureDirection.NORTH
                        else -> transformed
                    }
            }
            if (orientation == StructureDirection.WEST || orientation == StructureDirection.EAST) {
                transformed = transformed.rotate(org.basaltmc.world.structure.StructureRotation.CLOCKWISE_90)
            }
            return transformed
        }

        private fun box(
            chunk: Chunk,
            clip: StructureBoundingBox,
            x0: Int,
            y0: Int,
            z0: Int,
            x1: Int,
            y1: Int,
            z1: Int,
            stateId: Int,
        ) {
            for (y in y0..y1) for (x in x0..x1) for (z in z0..z1) placeBlock(chunk, clip, x, y, z, stateId)
        }

        private fun placeBlock(
            chunk: Chunk,
            clip: StructureBoundingBox,
            x: Int,
            y: Int,
            z: Int,
            stateId: Int,
        ) {
            val worldX = worldX(x, z)
            val worldY = boundingBox.minY + y
            val worldZ = worldZ(x, z)
            if (worldY in chunk.verticalBounds.minY..chunk.verticalBounds.maxY &&
                clip.isInside(
                    org.basaltmc.world.structure
                        .StructurePos(worldX, worldY, worldZ),
                )
            ) {
                chunk.setBlockStateId(worldX and 15, worldY, worldZ and 15, stateId)
            }
        }

        private fun fillDown(
            chunk: Chunk,
            clip: StructureBoundingBox,
            x: Int,
            startY: Int,
            z: Int,
            stateId: Int,
        ) {
            val worldX = worldX(x, z)
            val worldZ = worldZ(x, z)
            var worldY = boundingBox.minY + startY
            if (!clip.isInside(
                    org.basaltmc.world.structure
                        .StructurePos(worldX, worldY, worldZ),
                )
            ) {
                return
            }
            while (worldY > chunk.verticalBounds.minY + 1) {
                val current = chunk.getBlockStateId(worldX and 15, worldY, worldZ and 15)
                if (!replaceableByStructures(current)) break
                chunk.setBlockStateId(worldX and 15, worldY, worldZ and 15, stateId)
                worldY--
            }
        }

        private fun isInside(
            x: Int,
            y: Int,
            z: Int,
            clip: StructureBoundingBox,
        ): Boolean =
            clip.isInside(
                org.basaltmc.world.structure
                    .StructurePos(worldX(x, z), boundingBox.minY + y, worldZ(x, z)),
            )

        private fun blockAt(
            chunk: Chunk,
            x: Int,
            y: Int,
            z: Int,
        ): Int = chunk.getBlockStateId(worldX(x, z) and 15, boundingBox.minY + y, worldZ(x, z) and 15)

        private fun isChestAt(
            chunk: Chunk,
            x: Int,
            y: Int,
            z: Int,
        ): Boolean = Blocks.byStateId(blockAt(chunk, x, y, z))?.block?.path == "chest"

        private fun worldX(
            x: Int,
            z: Int,
        ): Int =
            when (orientation) {
                StructureDirection.NORTH, StructureDirection.SOUTH -> boundingBox.minX + x
                StructureDirection.WEST -> boundingBox.maxX - z
                StructureDirection.EAST -> boundingBox.minX + z
                else -> x
            }

        private fun worldZ(
            x: Int,
            z: Int,
        ): Int =
            when (orientation) {
                StructureDirection.NORTH -> boundingBox.maxZ - z
                StructureDirection.SOUTH -> boundingBox.minZ + z
                StructureDirection.WEST, StructureDirection.EAST -> boundingBox.minZ + x
                else -> z
            }
    }

    internal enum class PieceType {
        START,
        BRIDGE_CROSSING,
        BRIDGE_END_FILLER,
        BRIDGE_STRAIGHT,
        ROOM_CROSSING,
        STAIRS_ROOM,
        MONSTER_THRONE,
        CASTLE_ENTRANCE,
        CASTLE_SMALL_CORRIDOR,
        CASTLE_SMALL_CROSSING,
        CASTLE_LEFT_TURN,
        CASTLE_RIGHT_TURN,
        CASTLE_STAIRS,
        CASTLE_T_BALCONY,
        CASTLE_STALK_ROOM,
    }

    internal class AssemblyState(
        val start: Piece,
    ) {
        val pieces = mutableListOf<Piece>()
        val pending = mutableListOf<Piece>()
        val bridgeWeights = BRIDGE_WEIGHTS.map(PieceWeight::copy).toMutableList()
        val castleWeights = CASTLE_WEIGHTS.map(PieceWeight::copy).toMutableList()
        var previous: PieceWeight? = null
    }

    internal data class PieceWeight(
        val type: PieceType,
        val weight: Int,
        val maxCount: Int,
        val allowInRow: Boolean = false,
        var count: Int = 0,
    )

    private fun generatePiece(
        state: AssemblyState,
        available: MutableList<PieceWeight>,
        random: WorldgenRandom,
        x: Int,
        y: Int,
        z: Int,
        direction: StructureDirection,
        depth: Int,
    ): Piece? {
        val hasLimited = available.any { it.maxCount > 0 && it.count < it.maxCount }
        val totalWeight = if (hasLimited) available.sumOf(PieceWeight::weight) else -1
        repeat(5) {
            if (totalWeight <= 0 || depth > 30) return@repeat
            var selection = random.nextInt(totalWeight)
            for (weight in available) {
                selection -= weight.weight
                if (selection >= 0) continue
                if ((weight.maxCount > 0 && weight.count >= weight.maxCount) ||
                    weight === state.previous &&
                    !weight.allowInRow
                ) {
                    break
                }
                val piece = createPiece(weight.type, state, random, x, y, z, direction, depth)
                if (piece != null) {
                    weight.count++
                    state.previous = weight
                    if (weight.maxCount > 0 && weight.count >= weight.maxCount) available.remove(weight)
                    return piece
                }
            }
        }
        return createPiece(PieceType.BRIDGE_END_FILLER, state, random, x, y, z, direction, depth)
    }

    private fun createPiece(
        type: PieceType,
        state: AssemblyState,
        random: WorldgenRandom,
        x: Int,
        y: Int,
        z: Int,
        direction: StructureDirection,
        depth: Int,
    ): Piece? {
        val dimensions = PIECE_DIMENSIONS.getValue(type)
        val box = orientBox(x, y, z, dimensions, direction)
        if (box.minY <= 10 || state.pieces.any { it.boundingBox.intersects(box) }) return null
        return Piece(
            type,
            depth,
            box,
            direction,
            selfSeed = if (type == PieceType.BRIDGE_END_FILLER) random.nextInt() else null,
            needsChest =
                if (type == PieceType.CASTLE_LEFT_TURN ||
                    type == PieceType.CASTLE_RIGHT_TURN
                ) {
                    random.nextInt(3) == 0
                } else {
                    false
                },
        )
    }

    private data class Dimensions(
        val offX: Int,
        val offY: Int,
        val offZ: Int,
        val width: Int,
        val height: Int,
        val depth: Int,
    )

    private fun orientBox(
        x: Int,
        y: Int,
        z: Int,
        d: Dimensions,
        direction: StructureDirection,
    ): StructureBoundingBox =
        when (direction) {
            StructureDirection.NORTH ->
                StructureBoundingBox(
                    x + d.offX,
                    y + d.offY,
                    z - d.depth + 1 + d.offZ,
                    x + d.width - 1 + d.offX,
                    y + d.height - 1 + d.offY,
                    z + d.offZ,
                )
            StructureDirection.WEST ->
                StructureBoundingBox(
                    x - d.depth + 1 + d.offZ,
                    y + d.offY,
                    z + d.offX,
                    x + d.offZ,
                    y + d.height - 1 + d.offY,
                    z + d.width - 1 + d.offX,
                )
            StructureDirection.EAST ->
                StructureBoundingBox(
                    x + d.offZ,
                    y + d.offY,
                    z + d.offX,
                    x + d.depth - 1 + d.offZ,
                    y + d.height - 1 + d.offY,
                    z + d.width - 1 + d.offX,
                )
            else ->
                StructureBoundingBox(
                    x + d.offX,
                    y + d.offY,
                    z + d.offZ,
                    x + d.width - 1 + d.offX,
                    y + d.height - 1 + d.offY,
                    z + d.depth - 1 + d.offZ,
                )
        }

    private fun makeBoundingBox(
        x: Int,
        y: Int,
        z: Int,
        direction: StructureDirection,
        width: Int,
        height: Int,
        depth: Int,
    ): StructureBoundingBox =
        if (direction == StructureDirection.NORTH ||
            direction == StructureDirection.SOUTH
        ) {
            StructureBoundingBox(
                x,
                y,
                z,
                x + width - 1,
                y + height - 1,
                z + depth - 1,
            )
        } else {
            StructureBoundingBox(x, y, z, x + depth - 1, y + height - 1, z + width - 1)
        }

    private fun List<Piece>.boundingBox(): StructureBoundingBox =
        drop(1).fold(first().boundingBox) { a, piece ->
            val b = piece.boundingBox
            StructureBoundingBox(
                minOf(a.minX, b.minX),
                minOf(a.minY, b.minY),
                minOf(a.minZ, b.minZ),
                maxOf(a.maxX, b.maxX),
                maxOf(a.maxY, b.maxY),
                maxOf(a.maxZ, b.maxZ),
            )
        }

    private fun replaceableByStructures(stateId: Int): Boolean {
        if (Blocks.isAir(stateId) || Blocks.hasFluid(stateId)) return true
        return Blocks.byStateId(stateId)?.block?.path in REPLACEABLE_PATHS
    }

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId
            ?: error("missing vanilla block minecraft:$path")

    private fun stateId(
        path: String,
        properties: Map<String, String>,
    ): Int =
        Blocks.stateIdOf(Identifier.vanilla(path), properties)
            ?: error("missing vanilla block state minecraft:$path$properties")

    private val HORIZONTAL_DIRECTIONS =
        listOf(StructureDirection.NORTH, StructureDirection.EAST, StructureDirection.SOUTH, StructureDirection.WEST)
    private val REPLACEABLE_PATHS = setOf("glow_lichen", "seagrass", "tall_seagrass")
    private val AIR = Blocks.AIR.stateId
    private val NETHER_BRICKS = stateId("nether_bricks")
    private val NETHER_BRICK_FENCE = stateId("nether_brick_fence")
    private val SOUL_SAND = stateId("soul_sand")
    private val NETHER_WART = stateId("nether_wart")
    private val LAVA = Blocks.LAVA.stateId
    private val SPAWNER = stateId("spawner")

    private val BRIDGE_WEIGHTS =
        listOf(
            PieceWeight(PieceType.BRIDGE_STRAIGHT, 30, 0, true),
            PieceWeight(PieceType.BRIDGE_CROSSING, 10, 4),
            PieceWeight(PieceType.ROOM_CROSSING, 10, 4),
            PieceWeight(PieceType.STAIRS_ROOM, 10, 3),
            PieceWeight(PieceType.MONSTER_THRONE, 5, 2),
            PieceWeight(PieceType.CASTLE_ENTRANCE, 5, 1),
        )
    private val CASTLE_WEIGHTS =
        listOf(
            PieceWeight(PieceType.CASTLE_SMALL_CORRIDOR, 25, 0, true),
            PieceWeight(PieceType.CASTLE_SMALL_CROSSING, 15, 5),
            PieceWeight(PieceType.CASTLE_RIGHT_TURN, 5, 10),
            PieceWeight(PieceType.CASTLE_LEFT_TURN, 5, 10),
            PieceWeight(PieceType.CASTLE_STAIRS, 10, 3, true),
            PieceWeight(PieceType.CASTLE_T_BALCONY, 7, 2),
            PieceWeight(PieceType.CASTLE_STALK_ROOM, 5, 2),
        )
    private val PIECE_DIMENSIONS =
        mapOf(
            PieceType.BRIDGE_CROSSING to Dimensions(-8, -3, 0, 19, 10, 19),
            PieceType.BRIDGE_END_FILLER to Dimensions(-1, -3, 0, 5, 10, 8),
            PieceType.BRIDGE_STRAIGHT to Dimensions(-1, -3, 0, 5, 10, 19),
            PieceType.CASTLE_STAIRS to Dimensions(-1, -7, 0, 5, 14, 10),
            PieceType.CASTLE_T_BALCONY to Dimensions(-3, 0, 0, 9, 7, 9),
            PieceType.CASTLE_ENTRANCE to Dimensions(-5, -3, 0, 13, 14, 13),
            PieceType.CASTLE_SMALL_CROSSING to Dimensions(-1, 0, 0, 5, 7, 5),
            PieceType.CASTLE_LEFT_TURN to Dimensions(-1, 0, 0, 5, 7, 5),
            PieceType.CASTLE_SMALL_CORRIDOR to Dimensions(-1, 0, 0, 5, 7, 5),
            PieceType.CASTLE_RIGHT_TURN to Dimensions(-1, 0, 0, 5, 7, 5),
            PieceType.CASTLE_STALK_ROOM to Dimensions(-5, -3, 0, 13, 14, 13),
            PieceType.MONSTER_THRONE to Dimensions(-2, 0, 0, 7, 8, 9),
            PieceType.ROOM_CROSSING to Dimensions(-2, 0, 0, 7, 9, 7),
            PieceType.STAIRS_ROOM to Dimensions(-2, 0, 0, 7, 11, 7),
        )
}
