package org.basaltmc.server.world.structure

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom

/** // parity: MineshaftStructure.Type. Decides the wood a mineshaft is framed in. */
public enum class MineshaftType {
    NORMAL,
    MESA,
}

/** One assembled mineshaft piece: what it is, where it is, and the rolls its blocks will need. */
public data class MineshaftPiece(
    val kind: Kind,
    val box: StructureBoundingBox,
    /** The direction the piece was entered from. Null on the starting room, which has none. */
    val orientation: StructureDirection?,
    val genDepth: Int,
    val hasRails: Boolean = false,
    val spiderCorridor: Boolean = false,
    val sections: Int = 0,
    val twoFloored: Boolean = false,
    /** Only the room has these: the mouth it cut for each passage leaving it. */
    val entrances: List<StructureBoundingBox> = emptyList(),
) {
    /**
     * The orientation the blocks are written with, not always the one the piece was entered from.
     * // parity: only the corridor and the stairs call `setOrientation`.
     */
    public val fillOrientation: StructureDirection?
        get() = if (kind == Kind.CORRIDOR || kind == Kind.STAIRS) orientation else null

    public enum class Kind {
        CORRIDOR,
        CROSSING,
        STAIRS,
        ROOM,
    }
}

/**
 * // parity: MineshaftPieces plus MineshaftStructure.generatePiecesAndAdjust: which pieces exist,
 * where each sits and which way it faces. Split from the block filling so a layout can be compared
 * against a live world's `structures.starts` before a plank is placed.
 */
public object MineshaftLayout {
    /** The pieces of the mineshaft starting at [chunkX]/[chunkZ], in the order vanilla adds them. */
    public fun assemble(
        seed: Long,
        chunkX: Int,
        chunkZ: Int,
        type: MineshaftType,
        seaLevel: Int,
        minY: Int,
    ): List<MineshaftPiece> {
        val random = WorldgenRandom(LegacyRandom(0L))
        random.setLargeFeatureSeed(seed, chunkX, chunkZ)
        // parity: MineshaftStructure.findGenerationPoint throws this first draw away, and every
        // roll of the whole shaft hangs off what comes after it.
        random.nextDouble()
        val builder = Builder(random)
        builder.addRoom(chunkX * 16 + 2, chunkZ * 16 + 2, type)
        if (type == MineshaftType.MESA) return builder.pieces
        builder.moveBelowSeaLevel(seaLevel, minY)
        return builder.pieces
    }

    /**
     * // parity: StructurePiecesBuilder as MineshaftPieces uses it. Holds the pieces added so far so
     * a candidate box can be rejected for overlapping one, which is what keeps a shaft from folding
     * back into itself.
     */
    private class Builder(
        val random: WorldgenRandom,
    ) {
        val pieces = mutableListOf<MineshaftPiece>()

        fun collides(box: StructureBoundingBox): Boolean = pieces.any { it.box.intersects(box) }

        fun addRoom(
            west: Int,
            north: Int,
            type: MineshaftType,
        ) {
            val box =
                StructureBoundingBox(
                    west,
                    MAGIC_START_Y,
                    north,
                    west + 7 + random.nextInt(6),
                    MAGIC_START_Y + 4 + random.nextInt(6),
                    north + 7 + random.nextInt(6),
                )
            val room = MineshaftPiece(MineshaftPiece.Kind.ROOM, box, orientation = null, genDepth = 0)
            pieces += room
            val entrances = addRoomChildren(room)
            pieces[0] = pieces[0].copy(entrances = entrances)
        }

        fun moveBelowSeaLevel(
            seaLevel: Int,
            minY: Int,
        ) {
            val bounds = bounds() ?: return
            val maxY = seaLevel - SEA_LEVEL_MARGIN
            // getYSpan is inclusive, so the span is maxY - minY + 1 and not the difference.
            var top = (bounds.maxY - bounds.minY + 1) + minY + 1
            if (top < maxY) top += random.nextInt(maxY - top)
            val dy = top - bounds.maxY
            for (index in pieces.indices) {
                val piece = pieces[index]
                pieces[index] =
                    piece.copy(
                        box = piece.box.copy(minY = piece.box.minY + dy, maxY = piece.box.maxY + dy),
                        entrances = piece.entrances.map { it.copy(minY = it.minY + dy, maxY = it.maxY + dy) },
                    )
            }
        }

        private fun bounds(): StructureBoundingBox? {
            if (pieces.isEmpty()) return null
            var box = pieces.first().box
            for (piece in pieces.drop(1)) {
                box =
                    StructureBoundingBox(
                        minOf(box.minX, piece.box.minX),
                        minOf(box.minY, piece.box.minY),
                        minOf(box.minZ, piece.box.minZ),
                        maxOf(box.maxX, piece.box.maxX),
                        maxOf(box.maxY, piece.box.maxY),
                        maxOf(box.maxZ, piece.box.maxZ),
                    )
            }
            return box
        }

        /** // parity: MineshaftPieces.generateAndAddPiece, including its 80-block leash to the room. */
        fun generateAndAddPiece(
            start: MineshaftPiece,
            footX: Int,
            footY: Int,
            footZ: Int,
            direction: StructureDirection,
            depth: Int,
        ): MineshaftPiece? {
            if (depth > MAX_DEPTH) return null
            if (abs(footX - start.box.minX) > MAX_REACH || abs(footZ - start.box.minZ) > MAX_REACH) return null
            val piece = createRandomPiece(footX, footY, footZ, direction, depth + 1) ?: return null
            pieces += piece
            addChildren(start, piece)
            return piece
        }

        private fun createRandomPiece(
            footX: Int,
            footY: Int,
            footZ: Int,
            direction: StructureDirection,
            genDepth: Int,
        ): MineshaftPiece? {
            val selection = random.nextInt(100)
            return when {
                selection >= 80 ->
                    findCrossing(footX, footY, footZ, direction)?.let { box ->
                        MineshaftPiece(
                            MineshaftPiece.Kind.CROSSING,
                            box,
                            direction,
                            genDepth,
                            twoFloored = box.maxY - box.minY + 1 > 3,
                        )
                    }
                selection >= 70 ->
                    findStairs(footX, footY, footZ, direction)?.let { box ->
                        MineshaftPiece(MineshaftPiece.Kind.STAIRS, box, direction, genDepth)
                    }
                else -> findCorridor(footX, footY, footZ, direction)?.let { box -> corridor(box, direction, genDepth) }
            }
        }

        /** // parity: the MineShaftCorridor constructor, whose rolls come after its box is accepted. */
        private fun corridor(
            box: StructureBoundingBox,
            direction: StructureDirection,
            genDepth: Int,
        ): MineshaftPiece {
            val hasRails = random.nextInt(3) == 0
            val spider = !hasRails && random.nextInt(23) == 0
            val span =
                if (direction.axisIsZ) box.maxZ - box.minZ + 1 else box.maxX - box.minX + 1
            return MineshaftPiece(
                MineshaftPiece.Kind.CORRIDOR,
                box,
                direction,
                genDepth,
                hasRails = hasRails,
                spiderCorridor = spider,
                sections = span / 5,
            )
        }

        private fun findCorridor(
            footX: Int,
            footY: Int,
            footZ: Int,
            direction: StructureDirection,
        ): StructureBoundingBox? {
            var length = random.nextInt(3) + 2
            while (length > 0) {
                val blocks = length * 5
                val box =
                    when (direction) {
                        StructureDirection.SOUTH -> StructureBoundingBox(0, 0, 0, 2, 2, blocks - 1)
                        StructureDirection.WEST -> StructureBoundingBox(-(blocks - 1), 0, 0, 0, 2, 2)
                        StructureDirection.EAST -> StructureBoundingBox(0, 0, 0, blocks - 1, 2, 2)
                        else -> StructureBoundingBox(0, 0, -(blocks - 1), 2, 2, 0)
                    }.moved(footX, footY, footZ)
                if (!collides(box)) return box
                length--
            }
            return null
        }

        private fun findCrossing(
            footX: Int,
            footY: Int,
            footZ: Int,
            direction: StructureDirection,
        ): StructureBoundingBox? {
            val height = if (random.nextInt(4) == 0) 6 else 2
            val box =
                when (direction) {
                    StructureDirection.SOUTH -> StructureBoundingBox(-1, 0, 0, 3, height, 4)
                    StructureDirection.WEST -> StructureBoundingBox(-4, 0, -1, 0, height, 3)
                    StructureDirection.EAST -> StructureBoundingBox(0, 0, -1, 4, height, 3)
                    else -> StructureBoundingBox(-1, 0, -4, 3, height, 0)
                }.moved(footX, footY, footZ)
            return box.takeUnless { collides(it) }
        }

        private fun findStairs(
            footX: Int,
            footY: Int,
            footZ: Int,
            direction: StructureDirection,
        ): StructureBoundingBox? {
            val box =
                when (direction) {
                    StructureDirection.SOUTH -> StructureBoundingBox(0, -5, 0, 2, 2, 8)
                    StructureDirection.WEST -> StructureBoundingBox(-8, -5, 0, 0, 2, 2)
                    StructureDirection.EAST -> StructureBoundingBox(0, -5, 0, 8, 2, 2)
                    else -> StructureBoundingBox(0, -5, -8, 2, 2, 0)
                }.moved(footX, footY, footZ)
            return box.takeUnless { collides(it) }
        }

        private fun addChildren(
            start: MineshaftPiece,
            piece: MineshaftPiece,
        ) {
            when (piece.kind) {
                MineshaftPiece.Kind.CORRIDOR -> addCorridorChildren(start, piece)
                MineshaftPiece.Kind.CROSSING -> addCrossingChildren(start, piece)
                MineshaftPiece.Kind.STAIRS -> addStairsChildren(start, piece)
                MineshaftPiece.Kind.ROOM -> addRoomChildren(piece)
            }
        }

        private fun addCorridorChildren(
            start: MineshaftPiece,
            piece: MineshaftPiece,
        ) {
            val depth = piece.genDepth
            val box = piece.box
            val end = random.nextInt(4)

            fun ahead(
                x: Int,
                z: Int,
                direction: StructureDirection,
            ) = generateAndAddPiece(start, x, box.minY - 1 + random.nextInt(3), z, direction, depth)
            when (piece.orientation) {
                StructureDirection.SOUTH ->
                    when {
                        end <= 1 -> ahead(box.minX, box.maxZ + 1, StructureDirection.SOUTH)
                        end == 2 -> ahead(box.minX - 1, box.maxZ - 3, StructureDirection.WEST)
                        else -> ahead(box.maxX + 1, box.maxZ - 3, StructureDirection.EAST)
                    }
                StructureDirection.WEST ->
                    when {
                        end <= 1 -> ahead(box.minX - 1, box.minZ, StructureDirection.WEST)
                        end == 2 -> ahead(box.minX, box.minZ - 1, StructureDirection.NORTH)
                        else -> ahead(box.minX, box.maxZ + 1, StructureDirection.SOUTH)
                    }
                StructureDirection.EAST ->
                    when {
                        end <= 1 -> ahead(box.maxX + 1, box.minZ, StructureDirection.EAST)
                        end == 2 -> ahead(box.maxX - 3, box.minZ - 1, StructureDirection.NORTH)
                        else -> ahead(box.maxX - 3, box.maxZ + 1, StructureDirection.SOUTH)
                    }
                else ->
                    when {
                        end <= 1 -> ahead(box.minX, box.minZ - 1, StructureDirection.NORTH)
                        end == 2 -> ahead(box.minX - 1, box.minZ, StructureDirection.WEST)
                        else -> ahead(box.maxX + 1, box.minZ, StructureDirection.EAST)
                    }
            }
            if (depth >= MAX_DEPTH) return
            // Side passages every five blocks along the corridor, one roll each.
            if (piece.orientation?.axisIsZ != true) {
                var x = box.minX + 3
                while (x + 3 <= box.maxX) {
                    when (random.nextInt(5)) {
                        0 -> generateAndAddPiece(start, x, box.minY, box.minZ - 1, StructureDirection.NORTH, depth + 1)
                        1 -> generateAndAddPiece(start, x, box.minY, box.maxZ + 1, StructureDirection.SOUTH, depth + 1)
                        else -> Unit
                    }
                    x += 5
                }
            } else {
                var z = box.minZ + 3
                while (z + 3 <= box.maxZ) {
                    when (random.nextInt(5)) {
                        0 -> generateAndAddPiece(start, box.minX - 1, box.minY, z, StructureDirection.WEST, depth + 1)
                        1 -> generateAndAddPiece(start, box.maxX + 1, box.minY, z, StructureDirection.EAST, depth + 1)
                        else -> Unit
                    }
                    z += 5
                }
            }
        }

        private fun addCrossingChildren(
            start: MineshaftPiece,
            piece: MineshaftPiece,
        ) {
            val depth = piece.genDepth
            val box = piece.box
            when (piece.orientation) {
                StructureDirection.SOUTH -> {
                    generateAndAddPiece(start, box.minX + 1, box.minY, box.maxZ + 1, StructureDirection.SOUTH, depth)
                    generateAndAddPiece(start, box.minX - 1, box.minY, box.minZ + 1, StructureDirection.WEST, depth)
                    generateAndAddPiece(start, box.maxX + 1, box.minY, box.minZ + 1, StructureDirection.EAST, depth)
                }
                StructureDirection.WEST -> {
                    generateAndAddPiece(start, box.minX + 1, box.minY, box.minZ - 1, StructureDirection.NORTH, depth)
                    generateAndAddPiece(start, box.minX + 1, box.minY, box.maxZ + 1, StructureDirection.SOUTH, depth)
                    generateAndAddPiece(start, box.minX - 1, box.minY, box.minZ + 1, StructureDirection.WEST, depth)
                }
                StructureDirection.EAST -> {
                    generateAndAddPiece(start, box.minX + 1, box.minY, box.minZ - 1, StructureDirection.NORTH, depth)
                    generateAndAddPiece(start, box.minX + 1, box.minY, box.maxZ + 1, StructureDirection.SOUTH, depth)
                    generateAndAddPiece(start, box.maxX + 1, box.minY, box.minZ + 1, StructureDirection.EAST, depth)
                }
                else -> {
                    generateAndAddPiece(start, box.minX + 1, box.minY, box.minZ - 1, StructureDirection.NORTH, depth)
                    generateAndAddPiece(start, box.minX - 1, box.minY, box.minZ + 1, StructureDirection.WEST, depth)
                    generateAndAddPiece(start, box.maxX + 1, box.minY, box.minZ + 1, StructureDirection.EAST, depth)
                }
            }
            if (!piece.twoFloored) return
            val upper = box.minY + UPPER_FLOOR_OFFSET
            if (random.nextBoolean()) {
                generateAndAddPiece(start, box.minX + 1, upper, box.minZ - 1, StructureDirection.NORTH, depth)
            }
            if (random.nextBoolean()) {
                generateAndAddPiece(start, box.minX - 1, upper, box.minZ + 1, StructureDirection.WEST, depth)
            }
            if (random.nextBoolean()) {
                generateAndAddPiece(start, box.maxX + 1, upper, box.minZ + 1, StructureDirection.EAST, depth)
            }
            if (random.nextBoolean()) {
                generateAndAddPiece(start, box.minX + 1, upper, box.maxZ + 1, StructureDirection.SOUTH, depth)
            }
        }

        private fun addStairsChildren(
            start: MineshaftPiece,
            piece: MineshaftPiece,
        ) {
            val depth = piece.genDepth
            val box = piece.box
            when (piece.orientation) {
                StructureDirection.SOUTH ->
                    generateAndAddPiece(start, box.minX, box.minY, box.maxZ + 1, StructureDirection.SOUTH, depth)
                StructureDirection.WEST ->
                    generateAndAddPiece(start, box.minX - 1, box.minY, box.minZ, StructureDirection.WEST, depth)
                StructureDirection.EAST ->
                    generateAndAddPiece(start, box.maxX + 1, box.minY, box.minZ, StructureDirection.EAST, depth)
                else ->
                    generateAndAddPiece(start, box.minX, box.minY, box.minZ - 1, StructureDirection.NORTH, depth)
            }
        }

        /** Four passes around the room, each stepping along a wall and opening a passage or not. */
        private fun addRoomChildren(room: MineshaftPiece): List<StructureBoundingBox> {
            val entrances = mutableListOf<StructureBoundingBox>()
            val depth = room.genDepth
            val box = room.box
            val xSpan = box.maxX - box.minX + 1
            val zSpan = box.maxZ - box.minZ + 1
            val headroom = (box.maxY - box.minY + 1 - 3 - 1).coerceAtLeast(1)

            fun wall(
                span: Int,
                place: (Int, Int) -> MineshaftPiece?,
                mouth: (StructureBoundingBox) -> StructureBoundingBox,
            ) {
                var pos = 0
                while (pos < span) {
                    pos += random.nextInt(span)
                    if (pos + 3 > span) break
                    val child = place(pos, box.minY + random.nextInt(headroom) + 1)
                    if (child != null) entrances += mouth(child.box)
                    pos += 4
                }
            }

            wall(
                xSpan,
                {
                    pos,
                    y,
                    ->
                    generateAndAddPiece(room, box.minX + pos, y, box.minZ - 1, StructureDirection.NORTH, depth)
                },
                { c -> StructureBoundingBox(c.minX, c.minY, box.minZ, c.maxX, c.maxY, box.minZ + 1) },
            )
            wall(
                xSpan,
                {
                    pos,
                    y,
                    ->
                    generateAndAddPiece(room, box.minX + pos, y, box.maxZ + 1, StructureDirection.SOUTH, depth)
                },
                { c -> StructureBoundingBox(c.minX, c.minY, box.maxZ - 1, c.maxX, c.maxY, box.maxZ) },
            )
            wall(
                zSpan,
                {
                    pos,
                    y,
                    ->
                    generateAndAddPiece(room, box.minX - 1, y, box.minZ + pos, StructureDirection.WEST, depth)
                },
                { c -> StructureBoundingBox(box.minX, c.minY, c.minZ, box.minX + 1, c.maxY, c.maxZ) },
            )
            wall(
                zSpan,
                {
                    pos,
                    y,
                    ->
                    generateAndAddPiece(room, box.maxX + 1, y, box.minZ + pos, StructureDirection.EAST, depth)
                },
                { c -> StructureBoundingBox(box.maxX - 1, c.minY, c.minZ, box.maxX, c.maxY, c.maxZ) },
            )
            return entrances
        }
    }

    private fun StructureBoundingBox.moved(
        x: Int,
        y: Int,
        z: Int,
    ): StructureBoundingBox = StructureBoundingBox(minX + x, minY + y, minZ + z, maxX + x, maxY + y, maxZ + z)

    private val StructureDirection.axisIsZ: Boolean
        get() = this == StructureDirection.NORTH || this == StructureDirection.SOUTH

    private fun abs(value: Int): Int = if (value < 0) -value else value

    private const val MAGIC_START_Y = 50
    private const val MAX_DEPTH = 8
    private const val MAX_REACH = 80
    private const val SEA_LEVEL_MARGIN = 10
    private const val UPPER_FLOOR_OFFSET = 4
}
