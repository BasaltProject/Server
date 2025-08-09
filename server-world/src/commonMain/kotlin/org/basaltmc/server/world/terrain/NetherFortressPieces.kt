package org.basaltmc.server.world.terrain

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.hasFluid
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.structure.StructureBoundingBox
import org.basaltmc.server.world.structure.StructureDirection
import org.basaltmc.wire.common.Identifier

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
                    org.basaltmc.server.world.structure
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

    internal fun replaceableByStructures(stateId: Int): Boolean {
        if (Blocks.isAir(stateId) || Blocks.hasFluid(stateId)) return true
        return Blocks.byStateId(stateId)?.block?.path in REPLACEABLE_PATHS
    }

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId
            ?: error("missing vanilla block minecraft:$path")

    internal fun stateId(
        path: String,
        properties: Map<String, String>,
    ): Int =
        Blocks.stateIdOf(Identifier.vanilla(path), properties)
            ?: error("missing vanilla block state minecraft:$path$properties")

    private val HORIZONTAL_DIRECTIONS =
        listOf(StructureDirection.NORTH, StructureDirection.EAST, StructureDirection.SOUTH, StructureDirection.WEST)
    private val REPLACEABLE_PATHS = setOf("glow_lichen", "seagrass", "tall_seagrass")
    internal val AIR = Blocks.AIR.stateId
    internal val NETHER_BRICKS = stateId("nether_bricks")
    internal val NETHER_BRICK_FENCE = stateId("nether_brick_fence")
    internal val SOUL_SAND = stateId("soul_sand")
    internal val NETHER_WART = stateId("nether_wart")
    internal val LAVA = Blocks.LAVA.stateId
    internal val SPAWNER = stateId("spawner")

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
