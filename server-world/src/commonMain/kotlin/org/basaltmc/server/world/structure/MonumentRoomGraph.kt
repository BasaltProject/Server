package org.basaltmc.server.world.structure

import org.basaltmc.server.random.BasaltRandom

/**
 * // parity: OceanMonumentPieces.RoomDefinition. One cell of the monument's five by five by three
 * lattice, with the six neighbours it touches and which of those doorways survived.
 */
internal class MonumentRoom(
    val index: Int,
) {
    val connections: Array<MonumentRoom?> = arrayOfNulls(6)
    val hasOpening: BooleanArray = BooleanArray(6)
    var claimed: Boolean = false
    var isSource: Boolean = false
    private var scanIndex: Int = 0

    fun connect(
        direction: StructureDirection,
        other: MonumentRoom,
    ) {
        connections[direction.ordinal] = other
        other.connections[direction.opposite().ordinal] = this
    }

    fun updateOpenings() {
        for (i in 0 until 6) hasOpening[i] = connections[i] != null
    }

    /** Whether the entrance is still reachable from here through open doorways. */
    fun findSource(scan: Int): Boolean {
        if (isSource) return true
        scanIndex = scan
        for (i in 0 until 6) {
            val next = connections[i] ?: continue
            if (hasOpening[i] && next.scanIndex != scan && next.findSource(scan)) return true
        }
        return false
    }

    val isSpecial: Boolean get() = index >= 75

    fun countOpenings(): Int = hasOpening.count { it }
}

/** The lattice plus the two cells the rest of the monument is measured from. */
internal class MonumentRoomGraph(
    val rooms: List<MonumentRoom>,
    val sourceRoom: MonumentRoom,
    val coreRoom: MonumentRoom,
)

/**
 * // parity: MonumentBuilding.generateRoomGraph. Builds the lattice, hangs the roof and the two
 * wings off it, claims the eight cells the core room occupies, and then walks every room shutting
 * doorways at random as long as the entrance stays reachable from both sides of each one.
 */
internal fun generateMonumentRoomGraph(random: BasaltRandom): MonumentRoomGraph {
    val grid = arrayOfNulls<MonumentRoom>(75)
    for (x in 0 until 5) {
        for (z in 0 until 4) {
            val index = roomIndex(x, 0, z)
            grid[index] = MonumentRoom(index)
        }
    }
    for (x in 0 until 5) {
        for (z in 0 until 4) {
            val index = roomIndex(x, 1, z)
            grid[index] = MonumentRoom(index)
        }
    }
    for (x in 1 until 4) {
        for (z in 0 until 2) {
            val index = roomIndex(x, 2, z)
            grid[index] = MonumentRoom(index)
        }
    }

    val sourceRoom = requireNotNull(grid[SOURCE_INDEX])

    for (x in 0 until 5) {
        for (z in 0 until 5) {
            for (y in 0 until 3) {
                val room = grid[roomIndex(x, y, z)] ?: continue
                for (direction in ORDER) {
                    val nx = x + direction.dx
                    val ny = y + direction.dy
                    val nz = z + direction.dz
                    if (nx !in 0 until 5 || nz !in 0 until 5 || ny !in 0 until 3) continue
                    val neighbour = grid[roomIndex(nx, ny, nz)] ?: continue
                    // parity: a neighbour on the Z axis is linked through the *opposite* face. It
                    // reads like a slip and it is not one: the lattice is built with it.
                    if (nz == z) room.connect(direction, neighbour) else room.connect(direction.opposite(), neighbour)
                }
            }
        }
    }

    val roofRoom = MonumentRoom(PENTHOUSE_INDEX)
    val leftWing = MonumentRoom(LEFT_WING_INDEX)
    val rightWing = MonumentRoom(RIGHT_WING_INDEX)
    requireNotNull(grid[TOP_CONNECT_INDEX]).connect(StructureDirection.UP, roofRoom)
    requireNotNull(grid[LEFT_WING_CONNECT_INDEX]).connect(StructureDirection.SOUTH, leftWing)
    requireNotNull(grid[RIGHT_WING_CONNECT_INDEX]).connect(StructureDirection.SOUTH, rightWing)
    roofRoom.claimed = true
    leftWing.claimed = true
    rightWing.claimed = true
    sourceRoom.isSource = true

    val coreRoom = requireNotNull(grid[roomIndex(random.nextInt(4), 0, 2)])
    coreRoom.claimed = true
    val east = StructureDirection.EAST.ordinal
    val north = StructureDirection.NORTH.ordinal
    val up = StructureDirection.UP.ordinal
    coreRoom.connections[east]?.claimed = true
    coreRoom.connections[north]?.claimed = true
    coreRoom.connections[east]
        ?.connections
        ?.get(north)
        ?.claimed = true
    coreRoom.connections[up]?.claimed = true
    coreRoom.connections[east]
        ?.connections
        ?.get(up)
        ?.claimed = true
    coreRoom.connections[north]
        ?.connections
        ?.get(up)
        ?.claimed = true
    coreRoom.connections[east]
        ?.connections
        ?.get(north)
        ?.connections
        ?.get(up)
        ?.claimed = true

    val rooms = mutableListOf<MonumentRoom>()
    for (room in grid) {
        if (room == null) continue
        room.updateOpenings()
        rooms += room
    }
    roofRoom.updateOpenings()
    shuffle(rooms, random)

    var scan = 1
    for (room in rooms) {
        var closed = 0
        var attempts = 0
        while (closed < 2 && attempts < 5) {
            attempts++
            val face = random.nextInt(6)
            if (!room.hasOpening[face]) continue
            val neighbour = room.connections[face] ?: continue
            val back = ORDER[face].opposite().ordinal
            room.hasOpening[face] = false
            neighbour.hasOpening[back] = false
            // The doorway stays shut only if both rooms can still reach the entrance without it.
            if (room.findSource(scan++) && neighbour.findSource(scan++)) {
                closed++
            } else {
                room.hasOpening[face] = true
                neighbour.hasOpening[back] = true
            }
        }
    }

    rooms += roofRoom
    rooms += leftWing
    rooms += rightWing
    return MonumentRoomGraph(rooms, sourceRoom, coreRoom)
}

/** // parity: OceanMonumentPiece.getRoomIndex. */
internal fun roomIndex(
    x: Int,
    y: Int,
    z: Int,
): Int = y * 25 + z * 5 + x

/** // parity: Direction.values / get3DDataValue, which is down, up, north, south, west, east. */
private val ORDER =
    listOf(
        StructureDirection.DOWN,
        StructureDirection.UP,
        StructureDirection.NORTH,
        StructureDirection.SOUTH,
        StructureDirection.WEST,
        StructureDirection.EAST,
    )

internal val SOURCE_INDEX: Int = roomIndex(2, 0, 0)
internal val TOP_CONNECT_INDEX: Int = roomIndex(2, 2, 0)
internal val LEFT_WING_CONNECT_INDEX: Int = roomIndex(0, 1, 0)
internal val RIGHT_WING_CONNECT_INDEX: Int = roomIndex(4, 1, 0)
internal const val LEFT_WING_INDEX: Int = 1001
internal const val RIGHT_WING_INDEX: Int = 1002
internal const val PENTHOUSE_INDEX: Int = 1003

/** // parity: Util.shuffle. */
private fun shuffle(
    list: MutableList<MonumentRoom>,
    random: BasaltRandom,
) {
    for (i in list.size downTo 2) {
        val swapTo = random.nextInt(i)
        val held = list[i - 1]
        list[i - 1] = list[swapTo]
        list[swapTo] = held
    }
}
