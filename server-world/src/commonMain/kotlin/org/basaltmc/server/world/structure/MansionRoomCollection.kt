package org.basaltmc.server.world.structure

import org.basaltmc.server.random.BasaltRandom

/**
 * // parity: WoodlandMansionPieces.FloorRoomCollection. Which template goes in a room of each shape.
 * The third floor is the second floor's catalogue unchanged, exactly as `ThirdFloorRoomCollection`
 * inherits it without overriding anything.
 */
internal class MansionRoomCollection(
    private val groundFloor: Boolean,
) {
    fun room1X1(random: BasaltRandom): String =
        if (groundFloor) "1x1_a" + (random.nextInt(5) + 1) else "1x1_b" + (random.nextInt(5) + 1)

    fun room1X1Secret(random: BasaltRandom): String = "1x1_as" + (random.nextInt(4) + 1)

    fun room1X2SideEntrance(
        random: BasaltRandom,
        isStairs: Boolean,
    ): String =
        when {
            groundFloor -> "1x2_a" + (random.nextInt(9) + 1)
            isStairs -> "1x2_c_stairs"
            else -> "1x2_c" + (random.nextInt(4) + 1)
        }

    fun room1X2FrontEntrance(
        random: BasaltRandom,
        isStairs: Boolean,
    ): String =
        when {
            groundFloor -> "1x2_b" + (random.nextInt(5) + 1)
            isStairs -> "1x2_d_stairs"
            else -> "1x2_d" + (random.nextInt(5) + 1)
        }

    // parity: the upper floors draw `nextInt(1)`, which always gives 1x2_se1 and still costs a roll.
    fun room1X2Secret(random: BasaltRandom): String =
        if (groundFloor) "1x2_s" + (random.nextInt(2) + 1) else "1x2_se" + (random.nextInt(1) + 1)

    fun room2X2(random: BasaltRandom): String =
        if (groundFloor) "2x2_a" + (random.nextInt(4) + 1) else "2x2_b" + (random.nextInt(5) + 1)

    fun room2X2Secret(): String = "2x2_s1"

    companion object {
        /** Ground floor, upper floor, tower. // parity: the three collections, in that order. */
        val BY_FLOOR: List<MansionRoomCollection> =
            listOf(
                MansionRoomCollection(groundFloor = true),
                MansionRoomCollection(groundFloor = false),
                MansionRoomCollection(groundFloor = false),
            )
    }
}

/**
 * // parity: the four `addRoom*` methods of MansionPiecePlacer. Every combination of where the door
 * is and which way the room runs has its own offset, turn and mirror, and they are not derivable
 * from one another: the jar spells all sixteen out and so does this.
 */
internal class MansionRoomFiller(
    private val random: BasaltRandom,
) : MansionRooms {
    override fun addRoom1X1(
        pieces: MutableList<MansionPiece>,
        position: StructurePos,
        rotation: StructureRotation,
        doorDirection: StructureDirection?,
        floor: Int,
    ) {
        val rooms = MansionRoomCollection.BY_FLOOR[floor]
        // The draw for an ordinary room happens first and is thrown away when the door turns out to
        // be the secret one, so a secret room costs two rolls, not one.
        var template = rooms.room1X1(random)
        var pieceRotation = StructureRotation.NONE
        if (doorDirection != StructureDirection.EAST) {
            when (doorDirection) {
                StructureDirection.NORTH -> pieceRotation = pieceRotation.rotated(StructureRotation.COUNTERCLOCKWISE_90)
                StructureDirection.WEST -> pieceRotation = pieceRotation.rotated(StructureRotation.CLOCKWISE_180)
                StructureDirection.SOUTH -> pieceRotation = pieceRotation.rotated(StructureRotation.CLOCKWISE_90)
                else -> template = rooms.room1X1Secret(random)
            }
        }
        var zero = zeroPositionWithTransform(StructurePos(1, 0, 0), StructureMirror.NONE, pieceRotation, 7, 7)
        pieceRotation = pieceRotation.rotated(rotation)
        zero = zero.rotate(rotation)
        pieces += MansionPiece(template, position.offset(zero.x, 0, zero.z), pieceRotation)
    }

    override fun addRoom1X2(
        pieces: MutableList<MansionPiece>,
        position: StructurePos,
        rotation: StructureRotation,
        roomDirection: StructureDirection?,
        doorDirection: StructureDirection,
        floor: Int,
        isStairs: Boolean,
    ) {
        val rooms = MansionRoomCollection.BY_FLOOR[floor]
        val east = rotation.rotate(StructureDirection.EAST)
        val west = rotation.rotate(StructureDirection.WEST)
        val north = rotation.rotate(StructureDirection.NORTH)
        val south = rotation.rotate(StructureDirection.SOUTH)
        val d = doorDirection
        val r = roomDirection
        when {
            d == StructureDirection.EAST && r == StructureDirection.SOUTH ->
                pieces +=
                    MansionPiece(rooms.room1X2SideEntrance(random, isStairs), position.relative(east, 1), rotation)
            d == StructureDirection.EAST && r == StructureDirection.NORTH ->
                pieces +=
                    MansionPiece(
                        rooms.room1X2SideEntrance(random, isStairs),
                        position.relative(east, 1).relative(south, 6),
                        rotation,
                        StructureMirror.LEFT_RIGHT,
                    )
            d == StructureDirection.WEST && r == StructureDirection.NORTH ->
                pieces +=
                    MansionPiece(
                        rooms.room1X2SideEntrance(random, isStairs),
                        position.relative(east, 7).relative(south, 6),
                        rotation.rotated(StructureRotation.CLOCKWISE_180),
                    )
            d == StructureDirection.WEST && r == StructureDirection.SOUTH ->
                pieces +=
                    MansionPiece(
                        rooms.room1X2SideEntrance(random, isStairs),
                        position.relative(east, 7),
                        rotation,
                        StructureMirror.FRONT_BACK,
                    )
            d == StructureDirection.SOUTH && r == StructureDirection.EAST ->
                pieces +=
                    MansionPiece(
                        rooms.room1X2SideEntrance(random, isStairs),
                        position.relative(east, 1),
                        rotation.rotated(StructureRotation.CLOCKWISE_90),
                        StructureMirror.LEFT_RIGHT,
                    )
            d == StructureDirection.SOUTH && r == StructureDirection.WEST ->
                pieces +=
                    MansionPiece(
                        rooms.room1X2SideEntrance(random, isStairs),
                        position.relative(east, 7),
                        rotation.rotated(StructureRotation.CLOCKWISE_90),
                    )
            d == StructureDirection.NORTH && r == StructureDirection.WEST ->
                pieces +=
                    MansionPiece(
                        rooms.room1X2SideEntrance(random, isStairs),
                        position.relative(east, 7).relative(south, 6),
                        rotation.rotated(StructureRotation.CLOCKWISE_90),
                        StructureMirror.FRONT_BACK,
                    )
            d == StructureDirection.NORTH && r == StructureDirection.EAST ->
                pieces +=
                    MansionPiece(
                        rooms.room1X2SideEntrance(random, isStairs),
                        position.relative(east, 1).relative(south, 6),
                        rotation.rotated(StructureRotation.COUNTERCLOCKWISE_90),
                    )
            d == StructureDirection.SOUTH && r == StructureDirection.NORTH ->
                pieces +=
                    MansionPiece(
                        rooms.room1X2FrontEntrance(random, isStairs),
                        position.relative(east, 1).relative(north, 8),
                        rotation,
                    )
            d == StructureDirection.NORTH && r == StructureDirection.SOUTH ->
                pieces +=
                    MansionPiece(
                        rooms.room1X2FrontEntrance(random, isStairs),
                        position.relative(east, 7).relative(south, 14),
                        rotation.rotated(StructureRotation.CLOCKWISE_180),
                    )
            d == StructureDirection.WEST && r == StructureDirection.EAST ->
                pieces +=
                    MansionPiece(
                        rooms.room1X2FrontEntrance(random, isStairs),
                        position.relative(east, 15),
                        rotation.rotated(StructureRotation.CLOCKWISE_90),
                    )
            d == StructureDirection.EAST && r == StructureDirection.WEST ->
                pieces +=
                    MansionPiece(
                        rooms.room1X2FrontEntrance(random, isStairs),
                        position.relative(west, 7).relative(south, 6),
                        rotation.rotated(StructureRotation.COUNTERCLOCKWISE_90),
                    )
            d == StructureDirection.UP && r == StructureDirection.EAST ->
                pieces +=
                    MansionPiece(
                        rooms.room1X2Secret(random),
                        position.relative(east, 15),
                        rotation.rotated(StructureRotation.CLOCKWISE_90),
                    )
            d == StructureDirection.UP && r == StructureDirection.SOUTH ->
                pieces += MansionPiece(rooms.room1X2Secret(random), position.relative(east, 1), rotation)
        }
    }

    override fun addRoom2X2(
        pieces: MutableList<MansionPiece>,
        position: StructurePos,
        rotation: StructureRotation,
        roomDirection: StructureDirection,
        doorDirection: StructureDirection,
        floor: Int,
    ) {
        var east = 0
        var south = 0
        var pieceRotation = rotation
        var mirror = StructureMirror.NONE
        val d = doorDirection
        val r = roomDirection
        when {
            d == StructureDirection.EAST && r == StructureDirection.SOUTH -> east = -7
            d == StructureDirection.EAST && r == StructureDirection.NORTH -> {
                east = -7
                south = 6
                mirror = StructureMirror.LEFT_RIGHT
            }
            d == StructureDirection.NORTH && r == StructureDirection.EAST -> {
                east = 1
                south = 14
                pieceRotation = rotation.rotated(StructureRotation.COUNTERCLOCKWISE_90)
            }
            d == StructureDirection.NORTH && r == StructureDirection.WEST -> {
                east = 7
                south = 14
                pieceRotation = rotation.rotated(StructureRotation.COUNTERCLOCKWISE_90)
                mirror = StructureMirror.LEFT_RIGHT
            }
            d == StructureDirection.SOUTH && r == StructureDirection.WEST -> {
                east = 7
                south = -8
                pieceRotation = rotation.rotated(StructureRotation.CLOCKWISE_90)
            }
            d == StructureDirection.SOUTH && r == StructureDirection.EAST -> {
                east = 1
                south = -8
                pieceRotation = rotation.rotated(StructureRotation.CLOCKWISE_90)
                mirror = StructureMirror.LEFT_RIGHT
            }
            d == StructureDirection.WEST && r == StructureDirection.NORTH -> {
                east = 15
                south = 6
                pieceRotation = rotation.rotated(StructureRotation.CLOCKWISE_180)
            }
            d == StructureDirection.WEST && r == StructureDirection.SOUTH -> {
                east = 15
                mirror = StructureMirror.FRONT_BACK
            }
        }
        val target =
            position
                .relative(rotation.rotate(StructureDirection.EAST), east)
                .relative(rotation.rotate(StructureDirection.SOUTH), south)
        pieces += MansionPiece(MansionRoomCollection.BY_FLOOR[floor].room2X2(random), target, pieceRotation, mirror)
    }

    override fun addRoom2X2Secret(
        pieces: MutableList<MansionPiece>,
        position: StructurePos,
        rotation: StructureRotation,
        floor: Int,
    ) {
        val target = position.relative(rotation.rotate(StructureDirection.EAST), 1)
        pieces +=
            MansionPiece(MansionRoomCollection.BY_FLOOR[floor].room2X2Secret(), target, rotation, StructureMirror.NONE)
    }
}
