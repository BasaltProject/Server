package org.basaltmc.server.world.structure

import org.basaltmc.server.random.BasaltRandom

/** Which of the monument's room shapes a lattice cell turned into. */
internal enum class MonumentRoomKind {
    ENTRY,
    CORE,
    DOUBLE_X,
    DOUBLE_XY,
    DOUBLE_Y,
    DOUBLE_YZ,
    DOUBLE_Z,
    SIMPLE,
    SIMPLE_TOP,
    LEFT_WING,
    RIGHT_WING,
    PENTHOUSE,
}

/**
 * One room of a monument, chosen but not yet built. [design] is the variant a room draws for
 * itself: the simple room takes one of three, a wing takes the low bit of a single shared draw.
 */
internal data class MonumentRoomPiece(
    val kind: MonumentRoomKind,
    val room: MonumentRoom?,
    val design: Int = 0,
)

/**
 * // parity: MonumentBuilding's constructor. Offers every unclaimed cell to the seven fitters in the
 * jar's order and takes the first that fits: that order decides whether a pair of cells becomes one
 * double room or two singles, so it is not alphabetical and must not be tidied.
 */
internal fun fitMonumentRooms(
    graph: MonumentRoomGraph,
    random: BasaltRandom,
): List<MonumentRoomPiece> {
    val pieces = mutableListOf<MonumentRoomPiece>()
    graph.sourceRoom.claimed = true
    pieces += MonumentRoomPiece(MonumentRoomKind.ENTRY, graph.sourceRoom)
    pieces += MonumentRoomPiece(MonumentRoomKind.CORE, graph.coreRoom)

    for (room in graph.rooms) {
        if (room.claimed || room.isSpecial) continue
        val fitted = FITTERS.firstNotNullOfOrNull { it(room, random) } ?: continue
        pieces += fitted
    }

    // One draw for both wings, and the second takes the value plus one, so their designs differ.
    val wingDraw = random.nextInt()
    pieces += MonumentRoomPiece(MonumentRoomKind.LEFT_WING, null, wingDraw and 1)
    pieces += MonumentRoomPiece(MonumentRoomKind.RIGHT_WING, null, (wingDraw + 1) and 1)
    pieces += MonumentRoomPiece(MonumentRoomKind.PENTHOUSE, null)
    return pieces
}

private fun open(
    room: MonumentRoom,
    direction: StructureDirection,
): Boolean = room.hasOpening[direction.ordinal] && room.connections[direction.ordinal]?.claimed == false

private fun neighbour(
    room: MonumentRoom,
    direction: StructureDirection,
): MonumentRoom? = room.connections[direction.ordinal]

/** // parity: the seven fitters, in the order MonumentBuilding registers them. */
private val FITTERS: List<(MonumentRoom, BasaltRandom) -> MonumentRoomPiece?> =
    listOf(
        // Double on X and Y: the cell, its east neighbour and both of their ceilings.
        { room, _ ->
            val east = neighbour(room, StructureDirection.EAST)
            if (open(room, StructureDirection.EAST) &&
                open(room, StructureDirection.UP) &&
                east != null &&
                open(east, StructureDirection.UP)
            ) {
                room.claimed = true
                east.claimed = true
                neighbour(room, StructureDirection.UP)?.claimed = true
                neighbour(east, StructureDirection.UP)?.claimed = true
                MonumentRoomPiece(MonumentRoomKind.DOUBLE_XY, room)
            } else {
                null
            }
        },
        // Double on Y and Z, the same shape turned onto the north face.
        { room, _ ->
            val north = neighbour(room, StructureDirection.NORTH)
            if (open(room, StructureDirection.NORTH) &&
                open(room, StructureDirection.UP) &&
                north != null &&
                open(north, StructureDirection.UP)
            ) {
                room.claimed = true
                north.claimed = true
                neighbour(room, StructureDirection.UP)?.claimed = true
                neighbour(north, StructureDirection.UP)?.claimed = true
                MonumentRoomPiece(MonumentRoomKind.DOUBLE_YZ, room)
            } else {
                null
            }
        },
        // Double on Z. The jar falls back to the south neighbour here, but its own `fits` already
        // guarantees the north door is open and free, so that branch is unreachable; it is kept
        // because reading the jar and finding it missing would look like an omission.
        { room, _ ->
            if (!open(room, StructureDirection.NORTH)) {
                null
            } else {
                val anchor =
                    if (room.hasOpening[StructureDirection.NORTH.ordinal] &&
                        neighbour(room, StructureDirection.NORTH)?.claimed == false
                    ) {
                        room
                    } else {
                        neighbour(room, StructureDirection.SOUTH)
                    }
                if (anchor == null) {
                    null
                } else {
                    anchor.claimed = true
                    neighbour(anchor, StructureDirection.NORTH)?.claimed = true
                    MonumentRoomPiece(MonumentRoomKind.DOUBLE_Z, anchor)
                }
            }
        },
        { room, _ ->
            if (open(room, StructureDirection.EAST)) {
                room.claimed = true
                neighbour(room, StructureDirection.EAST)?.claimed = true
                MonumentRoomPiece(MonumentRoomKind.DOUBLE_X, room)
            } else {
                null
            }
        },
        { room, _ ->
            if (open(room, StructureDirection.UP)) {
                room.claimed = true
                neighbour(room, StructureDirection.UP)?.claimed = true
                MonumentRoomPiece(MonumentRoomKind.DOUBLE_Y, room)
            } else {
                null
            }
        },
        // A cell with no doorway at all but the floor becomes the little top room.
        { room, _ ->
            val sealed =
                !room.hasOpening[StructureDirection.WEST.ordinal] &&
                    !room.hasOpening[StructureDirection.EAST.ordinal] &&
                    !room.hasOpening[StructureDirection.NORTH.ordinal] &&
                    !room.hasOpening[StructureDirection.SOUTH.ordinal] &&
                    !room.hasOpening[StructureDirection.UP.ordinal]
            if (sealed) {
                room.claimed = true
                MonumentRoomPiece(MonumentRoomKind.SIMPLE_TOP, room)
            } else {
                null
            }
        },
        // The catch-all, and the only fitter that draws: one of three interiors.
        { room, random ->
            room.claimed = true
            MonumentRoomPiece(MonumentRoomKind.SIMPLE, room, random.nextInt(3))
        },
    )
