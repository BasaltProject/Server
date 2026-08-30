package org.basaltmc.server.world.structure

import org.basaltmc.server.random.BasaltRandom

/** // parity: StrongholdPieces' eleven piece classes, plus the filler and the start. */
internal enum class StrongholdPieceKind {
    START,
    STRAIGHT,
    PRISON_HALL,
    LEFT_TURN,
    RIGHT_TURN,
    ROOM_CROSSING,
    STRAIGHT_STAIRS_DOWN,
    STAIRS_DOWN,
    FIVE_CROSSING,
    CHEST_CORRIDOR,
    LIBRARY,
    PORTAL_ROOM,
    FILLER_CORRIDOR,
}

/** // parity: StrongholdPiece.SmallDoorType, the way a piece opens onto the one before it. */
internal enum class StrongholdDoor {
    OPENING,
    WOOD_DOOR,
    GRATES,
    IRON_DOOR,
}

/**
 * One room of the maze. The per-kind fields are the draws its constructor made, kept here because
 * the geometry needs them later and redrawing would move the shared random.
 */
internal class StrongholdPiece(
    val kind: StrongholdPieceKind,
    var box: StructureBoundingBox,
    val orientation: StructureDirection,
    val depth: Int,
    val entryDoor: StrongholdDoor,
    /** FIVE_CROSSING's four optional doorways. */
    val leftLow: Boolean = false,
    val leftHigh: Boolean = false,
    val rightLow: Boolean = false,
    val rightHigh: Boolean = false,
    /** STRAIGHT's two optional side doorways. */
    val leftChild: Boolean = false,
    val rightChild: Boolean = false,
    /** ROOM_CROSSING's interior, one of five. */
    val roomType: Int = 0,
    /** LIBRARY is two storeys when it had room for eleven. */
    val tall: Boolean = false,
    /** Only the start piece, which is the one that forces a five-way next. */
    val isSource: Boolean = false,
    /** FILLER_CORRIDOR's length along its own axis. */
    val steps: Int = 0,
)

/** How often a kind may appear, and from which depth. // parity: STRONGHOLD_PIECE_WEIGHTS. */
private class PieceWeight(
    val kind: StrongholdPieceKind,
    val weight: Int,
    val maxPlaceCount: Int,
    val minDepth: Int = 0,
) {
    var placeCount: Int = 0

    fun doPlace(depth: Int): Boolean = (maxPlaceCount == 0 || placeCount < maxPlaceCount) && depth > minDepth

    fun isValid(): Boolean = maxPlaceCount == 0 || placeCount < maxPlaceCount
}

/**
 * // parity: StrongholdStructure.generatePieces. The maze is grown breadth-first but in random order:
 * the pending list is drained by drawing an index, so the shape depends on the whole history. It is
 * retried, seed bumped by one each time, until a portal room appears.
 */
internal class StrongholdLayout(
    private val seaLevel: Int,
    private val minY: Int,
) {
    /** The pieces of the stronghold that starts in this chunk, already moved down to their depth. */
    fun build(
        random: BasaltRandom,
        reseed: (attempt: Int) -> Unit,
        chunkX: Int,
        chunkZ: Int,
    ): List<StrongholdPiece> {
        var attempt = 0
        while (true) {
            reseed(attempt++)
            val walk = Walk(random)
            val facing = horizontal(random)
            val start =
                StrongholdPiece(
                    StrongholdPieceKind.START,
                    makeBox(chunkX * 16 + 2, MAGIC_START_Y, chunkZ * 16 + 2, facing, 5, 11, 5),
                    facing,
                    depth = 0,
                    entryDoor = StrongholdDoor.OPENING,
                    isSource = true,
                )
            walk.pieces += start
            walk.addChildren(start)
            while (walk.pending.isNotEmpty()) {
                val index = random.nextInt(walk.pending.size)
                walk.addChildren(walk.pending.removeAt(index))
            }
            moveBelowSeaLevel(walk.pieces, random)
            if (walk.pieces.isNotEmpty() && walk.hasPortalRoom) return walk.pieces
        }
    }

    /**
     * // parity: StructurePiecesBuilder.moveBelowSeaLevel, which drops the whole maze so that its top
     * sits ten blocks under the sea, or lower still when there is room to spare.
     */
    private fun moveBelowSeaLevel(
        pieces: List<StrongholdPiece>,
        random: BasaltRandom,
    ) {
        if (pieces.isEmpty()) return
        var minYSeen = Int.MAX_VALUE
        var maxYSeen = Int.MIN_VALUE
        for (piece in pieces) {
            minYSeen = minOf(minYSeen, piece.box.minY)
            maxYSeen = maxOf(maxYSeen, piece.box.maxY)
        }
        val highest = seaLevel - BELOW_SEA_LEVEL
        var top = (maxYSeen - minYSeen + 1) + minY + 1
        if (top < highest) top += random.nextInt(highest - top)
        val dy = top - maxYSeen
        for (piece in pieces) {
            piece.box =
                StructureBoundingBox(
                    piece.box.minX,
                    piece.box.minY + dy,
                    piece.box.minZ,
                    piece.box.maxX,
                    piece.box.maxY + dy,
                    piece.box.maxZ,
                )
        }
    }

    /** One attempt at the maze: the pieces placed so far, plus the weights they have used up. */
    private inner class Walk(
        private val random: BasaltRandom,
    ) {
        val pieces = mutableListOf<StrongholdPiece>()
        val pending = mutableListOf<StrongholdPiece>()
        var hasPortalRoom = false
        private val weights = defaultWeights()
        private var previous: PieceWeight? = null
        private var imposed: StrongholdPieceKind? = null
        private var totalWeight = 0

        fun addChildren(piece: StrongholdPiece) {
            when (piece.kind) {
                StrongholdPieceKind.START, StrongholdPieceKind.STAIRS_DOWN -> {
                    if (piece.isSource) imposed = StrongholdPieceKind.FIVE_CROSSING
                    forward(piece, 1, 1)
                }
                StrongholdPieceKind.STRAIGHT -> {
                    forward(piece, 1, 1)
                    if (piece.leftChild) left(piece, 1, 2)
                    if (piece.rightChild) right(piece, 1, 2)
                }
                StrongholdPieceKind.PRISON_HALL,
                StrongholdPieceKind.CHEST_CORRIDOR,
                StrongholdPieceKind.STRAIGHT_STAIRS_DOWN,
                -> forward(piece, 1, 1)
                StrongholdPieceKind.LEFT_TURN -> {
                    if (piece.orientation != StructureDirection.NORTH && piece.orientation != StructureDirection.EAST) {
                        right(piece, 1, 1)
                    } else {
                        left(piece, 1, 1)
                    }
                }
                StrongholdPieceKind.RIGHT_TURN -> {
                    if (piece.orientation != StructureDirection.NORTH && piece.orientation != StructureDirection.EAST) {
                        left(piece, 1, 1)
                    } else {
                        right(piece, 1, 1)
                    }
                }
                StrongholdPieceKind.ROOM_CROSSING -> {
                    forward(piece, 4, 1)
                    left(piece, 1, 4)
                    right(piece, 1, 4)
                }
                StrongholdPieceKind.FIVE_CROSSING -> {
                    var lowOffset = 3
                    var highOffset = 5
                    if (piece.orientation == StructureDirection.WEST || piece.orientation == StructureDirection.NORTH) {
                        lowOffset = 8 - lowOffset
                        highOffset = 8 - highOffset
                    }
                    forward(piece, 5, 1)
                    if (piece.leftLow) left(piece, lowOffset, 1)
                    if (piece.leftHigh) left(piece, highOffset, 7)
                    if (piece.rightLow) right(piece, lowOffset, 1)
                    if (piece.rightHigh) right(piece, highOffset, 7)
                }
                StrongholdPieceKind.PORTAL_ROOM -> hasPortalRoom = true
                StrongholdPieceKind.LIBRARY, StrongholdPieceKind.FILLER_CORRIDOR -> Unit
            }
        }

        /** // parity: generateSmallDoorChildForward, which leaves through the face the piece points at. */
        private fun forward(
            piece: StrongholdPiece,
            xOff: Int,
            yOff: Int,
        ) {
            val box = piece.box
            when (piece.orientation) {
                StructureDirection.NORTH ->
                    grow(box.minX + xOff, box.minY + yOff, box.minZ - 1, StructureDirection.NORTH, piece.depth)
                StructureDirection.SOUTH ->
                    grow(box.minX + xOff, box.minY + yOff, box.maxZ + 1, StructureDirection.SOUTH, piece.depth)
                StructureDirection.WEST ->
                    grow(box.minX - 1, box.minY + yOff, box.minZ + xOff, StructureDirection.WEST, piece.depth)
                StructureDirection.EAST ->
                    grow(box.maxX + 1, box.minY + yOff, box.minZ + xOff, StructureDirection.EAST, piece.depth)
                else -> Unit
            }
        }

        /** // parity: generateSmallDoorChildLeft. North and south both leave through the west face. */
        private fun left(
            piece: StrongholdPiece,
            yOff: Int,
            zOff: Int,
        ) {
            val box = piece.box
            when (piece.orientation) {
                StructureDirection.NORTH, StructureDirection.SOUTH ->
                    grow(box.minX - 1, box.minY + yOff, box.minZ + zOff, StructureDirection.WEST, piece.depth)
                StructureDirection.WEST ->
                    grow(box.minX + zOff, box.minY + yOff, box.minZ - 1, StructureDirection.NORTH, piece.depth)
                StructureDirection.EAST ->
                    grow(box.minX + zOff, box.minY + yOff, box.minZ - 1, StructureDirection.NORTH, piece.depth)
                else -> Unit
            }
        }

        /** // parity: generateSmallDoorChildRight, the mirror of the one above. */
        private fun right(
            piece: StrongholdPiece,
            yOff: Int,
            zOff: Int,
        ) {
            val box = piece.box
            when (piece.orientation) {
                StructureDirection.NORTH, StructureDirection.SOUTH ->
                    grow(box.maxX + 1, box.minY + yOff, box.minZ + zOff, StructureDirection.EAST, piece.depth)
                StructureDirection.WEST ->
                    grow(box.minX + zOff, box.minY + yOff, box.maxZ + 1, StructureDirection.SOUTH, piece.depth)
                StructureDirection.EAST ->
                    grow(box.minX + zOff, box.minY + yOff, box.maxZ + 1, StructureDirection.SOUTH, piece.depth)
                else -> Unit
            }
        }

        /** // parity: generateAndAddPiece, with its 112-block leash and its depth ceiling. */
        private fun grow(
            footX: Int,
            footY: Int,
            footZ: Int,
            direction: StructureDirection,
            depth: Int,
        ) {
            if (depth > MAX_DEPTH) return
            val start = pieces.first().box
            if (kotlin.math.abs(footX - start.minX) > LEASH) return
            if (kotlin.math.abs(footZ - start.minZ) > LEASH) return
            val piece = pieceFromSmallDoor(footX, footY, footZ, direction, depth + 1) ?: return
            pieces += piece
            pending += piece
        }

        /**
         * // parity: generatePieceFromSmallDoor. Five attempts at the weighted table, each of which
         * gives up the moment its chosen kind is refused, and then a filler corridor.
         */
        private fun pieceFromSmallDoor(
            footX: Int,
            footY: Int,
            footZ: Int,
            direction: StructureDirection,
            depth: Int,
        ): StrongholdPiece? {
            if (!updateWeights()) return null
            imposed?.let { kind ->
                imposed = null
                create(kind, footX, footY, footZ, direction, depth)?.let { return it }
            }
            var attempts = 0
            while (attempts < 5) {
                attempts++
                var selection = random.nextInt(totalWeight)
                for (entry in weights.toList()) {
                    selection -= entry.weight
                    if (selection >= 0) continue
                    if (!entry.doPlace(depth) || entry === previous) break
                    // parity: a kind that does not fit does *not* end the attempt. The running total
                    // stays negative, so every remaining kind is tried in table order until one fits.
                    val piece = create(entry.kind, footX, footY, footZ, direction, depth) ?: continue
                    entry.placeCount++
                    previous = entry
                    if (!entry.isValid()) weights.remove(entry)
                    return piece
                }
            }
            val box = fillerBox(footX, footY, footZ, direction) ?: return null
            if (box.minY <= 1) return null
            return StrongholdPiece(
                StrongholdPieceKind.FILLER_CORRIDOR,
                box,
                direction,
                depth,
                StrongholdDoor.OPENING,
                steps =
                    if (direction == StructureDirection.NORTH || direction == StructureDirection.SOUTH) {
                        box.maxZ - box.minZ + 1
                    } else {
                        box.maxX - box.minX + 1
                    },
            )
        }

        /** // parity: updatePieceWeight, which also refuses to go on once no capped kind is left. */
        private fun updateWeights(): Boolean {
            var any = false
            totalWeight = 0
            for (entry in weights) {
                if (entry.maxPlaceCount > 0 && entry.placeCount < entry.maxPlaceCount) any = true
                totalWeight += entry.weight
            }
            return any
        }

        /** // parity: each kind's `createPiece`: build the box, refuse it below y 10 or on a collision. */
        private fun create(
            kind: StrongholdPieceKind,
            footX: Int,
            footY: Int,
            footZ: Int,
            direction: StructureDirection,
            depth: Int,
        ): StrongholdPiece? {
            fun box(
                offX: Int,
                offY: Int,
                width: Int,
                height: Int,
                pieceDepth: Int,
            ) = orientBox(footX, footY, footZ, offX, offY, 0, width, height, pieceDepth, direction)

            fun free(candidate: StructureBoundingBox) = candidate.minY > LOWEST_Y && collision(candidate) == null

            return when (kind) {
                StrongholdPieceKind.STRAIGHT -> {
                    val candidate = box(-1, -1, 5, 5, 7)
                    if (!free(candidate)) {
                        null
                    } else {
                        val door = randomDoor()
                        StrongholdPiece(
                            kind,
                            candidate,
                            direction,
                            depth,
                            door,
                            leftChild = random.nextInt(2) == 0,
                            rightChild = random.nextInt(2) == 0,
                        )
                    }
                }
                StrongholdPieceKind.PRISON_HALL -> simple(kind, box(-1, -1, 9, 5, 11), direction, depth)
                StrongholdPieceKind.LEFT_TURN -> simple(kind, box(-1, -1, 5, 5, 5), direction, depth)
                StrongholdPieceKind.RIGHT_TURN -> simple(kind, box(-1, -1, 5, 5, 5), direction, depth)
                StrongholdPieceKind.ROOM_CROSSING -> {
                    val candidate = box(-4, -1, 11, 7, 11)
                    if (!free(candidate)) {
                        null
                    } else {
                        val door = randomDoor()
                        StrongholdPiece(kind, candidate, direction, depth, door, roomType = random.nextInt(5))
                    }
                }
                StrongholdPieceKind.STRAIGHT_STAIRS_DOWN -> simple(kind, box(-1, -7, 5, 11, 8), direction, depth)
                StrongholdPieceKind.STAIRS_DOWN -> simple(kind, box(-1, -7, 5, 11, 5), direction, depth)
                StrongholdPieceKind.FIVE_CROSSING -> {
                    val candidate = box(-4, -3, 10, 9, 11)
                    if (!free(candidate)) {
                        null
                    } else {
                        val door = randomDoor()
                        StrongholdPiece(
                            kind,
                            candidate,
                            direction,
                            depth,
                            door,
                            leftLow = random.nextBoolean(),
                            leftHigh = random.nextBoolean(),
                            rightLow = random.nextBoolean(),
                            rightHigh = random.nextInt(3) > 0,
                        )
                    }
                }
                StrongholdPieceKind.CHEST_CORRIDOR -> simple(kind, box(-1, -1, 5, 5, 7), direction, depth)
                StrongholdPieceKind.LIBRARY -> {
                    // parity: the tall library is tried first and the short one is the fallback, so a
                    // library that fits only at six blocks still counts against the cap.
                    var candidate = box(-4, -1, 14, 11, 15)
                    if (!free(candidate)) {
                        candidate = box(-4, -1, 14, 6, 15)
                        if (!free(candidate)) return null
                    }
                    val door = randomDoor()
                    StrongholdPiece(
                        kind,
                        candidate,
                        direction,
                        depth,
                        door,
                        tall = candidate.maxY - candidate.minY + 1 > 6,
                    )
                }
                StrongholdPieceKind.PORTAL_ROOM -> {
                    // parity: the portal room is the only kind whose constructor draws nothing.
                    val candidate = box(-4, -1, 11, 8, 16)
                    if (free(candidate)) {
                        StrongholdPiece(kind, candidate, direction, depth, StrongholdDoor.OPENING)
                    } else {
                        null
                    }
                }
                else -> null
            }
        }

        private fun simple(
            kind: StrongholdPieceKind,
            candidate: StructureBoundingBox,
            direction: StructureDirection,
            depth: Int,
        ): StrongholdPiece? {
            if (candidate.minY <= LOWEST_Y || collision(candidate) != null) return null
            return StrongholdPiece(kind, candidate, direction, depth, randomDoor())
        }

        /** // parity: randomSmallDoor: two of the five draws are the plain opening. */
        private fun randomDoor(): StrongholdDoor =
            when (random.nextInt(5)) {
                2 -> StrongholdDoor.WOOD_DOOR
                3 -> StrongholdDoor.GRATES
                4 -> StrongholdDoor.IRON_DOOR
                else -> StrongholdDoor.OPENING
            }

        /**
         * // parity: FillerCorridor.findPieceBox. A filler is only laid where something is already in
         * the way at the same height, and it is shortened until it just touches it.
         */
        private fun fillerBox(
            footX: Int,
            footY: Int,
            footZ: Int,
            direction: StructureDirection,
        ): StructureBoundingBox? {
            val full = orientBox(footX, footY, footZ, -1, -1, 0, 5, 5, 4, direction)
            val hit = collision(full) ?: return null
            if (hit.minY != full.minY) return null
            for (length in 2 downTo 1) {
                val shorter = orientBox(footX, footY, footZ, -1, -1, 0, 5, 5, length, direction)
                if (!hit.intersects(shorter)) {
                    return orientBox(footX, footY, footZ, -1, -1, 0, 5, 5, length + 1, direction)
                }
            }
            return null
        }

        private fun collision(box: StructureBoundingBox): StructureBoundingBox? =
            pieces.firstOrNull { it.box.intersects(box) }?.box
    }

    private companion object {
        const val MAGIC_START_Y = 64
        const val MAX_DEPTH = 50
        const val LOWEST_Y = 10
        const val LEASH = 112
        const val BELOW_SEA_LEVEL = 10

        val HORIZONTAL =
            listOf(
                StructureDirection.NORTH,
                StructureDirection.EAST,
                StructureDirection.SOUTH,
                StructureDirection.WEST,
            )

        fun horizontal(random: BasaltRandom): StructureDirection = HORIZONTAL[random.nextInt(HORIZONTAL.size)]

        /** // parity: StructurePiece.makeBoundingBox, which swaps width and depth on the X axis. */
        fun makeBox(
            x: Int,
            y: Int,
            z: Int,
            direction: StructureDirection,
            width: Int,
            height: Int,
            depth: Int,
        ): StructureBoundingBox =
            if (direction == StructureDirection.NORTH || direction == StructureDirection.SOUTH) {
                StructureBoundingBox(x, y, z, x + width - 1, y + height - 1, z + depth - 1)
            } else {
                StructureBoundingBox(x, y, z, x + depth - 1, y + height - 1, z + width - 1)
            }

        /** // parity: BoundingBox.orientBox, which grows the box away from the door it hangs off. */
        fun orientBox(
            footX: Int,
            footY: Int,
            footZ: Int,
            offX: Int,
            offY: Int,
            offZ: Int,
            width: Int,
            height: Int,
            depth: Int,
            direction: StructureDirection,
        ): StructureBoundingBox =
            when (direction) {
                StructureDirection.NORTH ->
                    StructureBoundingBox(
                        footX + offX,
                        footY + offY,
                        footZ - depth + 1 + offZ,
                        footX + width - 1 + offX,
                        footY + height - 1 + offY,
                        footZ + offZ,
                    )
                StructureDirection.WEST ->
                    StructureBoundingBox(
                        footX - depth + 1 + offZ,
                        footY + offY,
                        footZ + offX,
                        footX + offZ,
                        footY + height - 1 + offY,
                        footZ + width - 1 + offX,
                    )
                StructureDirection.EAST ->
                    StructureBoundingBox(
                        footX + offZ,
                        footY + offY,
                        footZ + offX,
                        footX + depth - 1 + offZ,
                        footY + height - 1 + offY,
                        footZ + width - 1 + offX,
                    )
                else ->
                    StructureBoundingBox(
                        footX + offX,
                        footY + offY,
                        footZ + offZ,
                        footX + width - 1 + offX,
                        footY + height - 1 + offY,
                        footZ + depth - 1 + offZ,
                    )
            }

        fun defaultWeights(): MutableList<PieceWeight> =
            mutableListOf(
                PieceWeight(StrongholdPieceKind.STRAIGHT, 40, 0),
                PieceWeight(StrongholdPieceKind.PRISON_HALL, 5, 5),
                PieceWeight(StrongholdPieceKind.LEFT_TURN, 20, 0),
                PieceWeight(StrongholdPieceKind.RIGHT_TURN, 20, 0),
                PieceWeight(StrongholdPieceKind.ROOM_CROSSING, 10, 6),
                PieceWeight(StrongholdPieceKind.STRAIGHT_STAIRS_DOWN, 5, 5),
                PieceWeight(StrongholdPieceKind.STAIRS_DOWN, 5, 5),
                PieceWeight(StrongholdPieceKind.FIVE_CROSSING, 5, 4),
                PieceWeight(StrongholdPieceKind.CHEST_CORRIDOR, 5, 4),
                PieceWeight(StrongholdPieceKind.LIBRARY, 10, 2, minDepth = 4),
                PieceWeight(StrongholdPieceKind.PORTAL_ROOM, 20, 1, minDepth = 5),
            )
    }
}
