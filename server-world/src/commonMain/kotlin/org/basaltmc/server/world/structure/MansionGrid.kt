package org.basaltmc.server.world.structure

import org.basaltmc.server.random.BasaltRandom

/**
 * // parity: WoodlandMansionPieces.SimpleGrid. Reads outside its bounds answer [valueIfOutside]
 * instead of failing, which the mansion layout leans on constantly.
 */
internal class SimpleGrid(
    val width: Int,
    val height: Int,
    private val valueIfOutside: Int,
) {
    private val cells = IntArray(width * height)

    operator fun get(
        x: Int,
        y: Int,
    ): Int = if (x in 0 until width && y in 0 until height) cells[x + y * width] else valueIfOutside

    fun set(
        x: Int,
        y: Int,
        value: Int,
    ) {
        if (x in 0 until width && y in 0 until height) cells[x + y * width] = value
    }

    fun set(
        x0: Int,
        y0: Int,
        x1: Int,
        y1: Int,
        value: Int,
    ) {
        for (y in y0..y1) {
            for (x in x0..x1) set(x, y, value)
        }
    }

    fun setIf(
        x: Int,
        y: Int,
        ifValue: Int,
        value: Int,
    ) {
        if (get(x, y) == ifValue) set(x, y, value)
    }

    fun edgesTo(
        x: Int,
        y: Int,
        ifValue: Int,
    ): Boolean =
        get(x - 1, y) == ifValue || get(x + 1, y) == ifValue || get(x, y + 1) == ifValue || get(x, y - 1) == ifValue
}

/**
 * // parity: WoodlandMansionPieces.MansionGrid, the floor plan on an eleven by eleven board, packed
 * into one int per cell with the room's size, id and flags. The second coordinate is called `y` here
 * as in the jar, but it is the Z axis.
 */
internal class MansionGrid(
    private val random: BasaltRandom,
) {
    val baseGrid: SimpleGrid = SimpleGrid(SIZE, SIZE, BLOCKED)
    val thirdFloorGrid: SimpleGrid = SimpleGrid(SIZE, SIZE, BLOCKED)
    val floorRooms: Array<SimpleGrid> = Array(3) { SimpleGrid(SIZE, SIZE, BLOCKED) }

    init {
        baseGrid.set(ENTRANCE_X, ENTRANCE_Y, ENTRANCE_X + 1, ENTRANCE_Y + 1, START_ROOM)
        baseGrid.set(ENTRANCE_X - 1, ENTRANCE_Y, ENTRANCE_X - 1, ENTRANCE_Y + 1, ROOM)
        baseGrid.set(ENTRANCE_X + 2, ENTRANCE_Y - 2, ENTRANCE_X + 3, ENTRANCE_Y + 3, BLOCKED)
        baseGrid.set(ENTRANCE_X + 1, ENTRANCE_Y - 2, ENTRANCE_X + 1, ENTRANCE_Y - 1, CORRIDOR)
        baseGrid.set(ENTRANCE_X + 1, ENTRANCE_Y + 2, ENTRANCE_X + 1, ENTRANCE_Y + 3, CORRIDOR)
        baseGrid.set(ENTRANCE_X - 1, ENTRANCE_Y - 1, CORRIDOR)
        baseGrid.set(ENTRANCE_X - 1, ENTRANCE_Y + 2, CORRIDOR)
        baseGrid.set(0, 0, SIZE, 1, BLOCKED)
        baseGrid.set(0, 9, SIZE, SIZE, BLOCKED)
        recursiveCorridor(baseGrid, ENTRANCE_X, ENTRANCE_Y - 2, StructureDirection.WEST, 6)
        recursiveCorridor(baseGrid, ENTRANCE_X, ENTRANCE_Y + 3, StructureDirection.WEST, 6)
        recursiveCorridor(baseGrid, ENTRANCE_X - 2, ENTRANCE_Y - 1, StructureDirection.WEST, 3)
        recursiveCorridor(baseGrid, ENTRANCE_X - 2, ENTRANCE_Y + 2, StructureDirection.WEST, 3)

        @Suppress("ControlFlowWithEmptyBody")
        while (cleanEdges(baseGrid)) {
        }

        identifyRooms(baseGrid, floorRooms[0])
        identifyRooms(baseGrid, floorRooms[1])
        floorRooms[0].set(ENTRANCE_X + 1, ENTRANCE_Y, ENTRANCE_X + 1, ENTRANCE_Y + 1, ROOM_CORRIDOR_FLAG)
        floorRooms[1].set(ENTRANCE_X + 1, ENTRANCE_Y, ENTRANCE_X + 1, ENTRANCE_Y + 1, ROOM_CORRIDOR_FLAG)
        setupThirdFloor()
        identifyRooms(thirdFloorGrid, floorRooms[2])
    }

    fun isRoomId(
        x: Int,
        y: Int,
        floor: Int,
        roomId: Int,
    ): Boolean = (floorRooms[floor][x, y] and ROOM_ID_MASK) == roomId

    /** The neighbour a 1x2 room extends into, or null when the cell is not part of one. */
    fun roomDirection1x2(
        x: Int,
        y: Int,
        floor: Int,
        roomId: Int,
    ): StructureDirection? = HORIZONTAL.firstOrNull { isRoomId(x + it.dx, y + it.dz, floor, roomId) }

    private fun recursiveCorridor(
        grid: SimpleGrid,
        x: Int,
        y: Int,
        heading: StructureDirection,
        depth: Int,
    ) {
        if (depth <= 0) return
        grid.set(x, y, CORRIDOR)
        grid.setIf(x + heading.dx, y + heading.dz, CLEAR, CORRIDOR)

        // Eight tries at a turn, and the draw happens on every one of them even when the branch is
        // refused, so an early `continue` would move every roll after it.
        for (attempt in 0 until 8) {
            val next = BY_2D[random.nextInt(4)]
            if (next != heading.opposite() && (next != StructureDirection.EAST || !random.nextBoolean())) {
                val nx = x + heading.dx
                val ny = y + heading.dz
                if (grid[nx + next.dx, ny + next.dz] == CLEAR && grid[nx + next.dx * 2, ny + next.dz * 2] == CLEAR) {
                    recursiveCorridor(grid, x + heading.dx + next.dx, y + heading.dz + next.dz, next, depth - 1)
                    break
                }
            }
        }

        val cw = clockWise(heading)
        val ccw = counterClockWise(heading)
        grid.setIf(x + cw.dx, y + cw.dz, CLEAR, ROOM)
        grid.setIf(x + ccw.dx, y + ccw.dz, CLEAR, ROOM)
        grid.setIf(x + heading.dx + cw.dx, y + heading.dz + cw.dz, CLEAR, ROOM)
        grid.setIf(x + heading.dx + ccw.dx, y + heading.dz + ccw.dz, CLEAR, ROOM)
        grid.setIf(x + heading.dx * 2, y + heading.dz * 2, CLEAR, ROOM)
        grid.setIf(x + cw.dx * 2, y + cw.dz * 2, CLEAR, ROOM)
        grid.setIf(x + ccw.dx * 2, y + ccw.dz * 2, CLEAR, ROOM)
    }

    /** Fills in the dents an L of corridors leaves, until nothing changes. */
    private fun cleanEdges(grid: SimpleGrid): Boolean {
        var touched = false
        for (y in 0 until grid.height) {
            for (x in 0 until grid.width) {
                if (grid[x, y] != CLEAR) continue
                var direct = 0
                if (isHouse(grid, x + 1, y)) direct++
                if (isHouse(grid, x - 1, y)) direct++
                if (isHouse(grid, x, y + 1)) direct++
                if (isHouse(grid, x, y - 1)) direct++
                if (direct >= 3) {
                    grid.set(x, y, ROOM)
                    touched = true
                } else if (direct == 2) {
                    var diagonal = 0
                    if (isHouse(grid, x + 1, y + 1)) diagonal++
                    if (isHouse(grid, x - 1, y + 1)) diagonal++
                    if (isHouse(grid, x + 1, y - 1)) diagonal++
                    if (isHouse(grid, x - 1, y - 1)) diagonal++
                    if (diagonal <= 1) {
                        grid.set(x, y, ROOM)
                        touched = true
                    }
                }
            }
        }
        return touched
    }

    /** The tower: one 1x2 room of the second floor grows a staircase and a short corridor. */
    private fun setupThirdFloor() {
        val potential = mutableListOf<Pair<Int, Int>>()
        val floor = floorRooms[1]
        for (y in 0 until thirdFloorGrid.height) {
            for (x in 0 until thirdFloorGrid.width) {
                val data = floor[x, y]
                if (data and ROOM_TYPE_MASK == ROOM_1X2 && data and ROOM_DOOR_FLAG == ROOM_DOOR_FLAG) {
                    potential += x to y
                }
            }
        }
        if (potential.isEmpty()) {
            thirdFloorGrid.set(0, 0, thirdFloorGrid.width, thirdFloorGrid.height, BLOCKED)
            return
        }
        val (roomX, roomY) = potential[random.nextInt(potential.size)]
        val data = floor[roomX, roomY]
        floor.set(roomX, roomY, data or ROOM_STAIRS_FLAG)
        val direction = roomDirection1x2(roomX, roomY, 1, data and ROOM_ID_MASK) ?: return
        val endX = roomX + direction.dx
        val endY = roomY + direction.dz

        for (y in 0 until thirdFloorGrid.height) {
            for (x in 0 until thirdFloorGrid.width) {
                if (!isHouse(baseGrid, x, y)) {
                    thirdFloorGrid.set(x, y, BLOCKED)
                } else if (x == roomX && y == roomY) {
                    thirdFloorGrid.set(x, y, START_ROOM)
                } else if (x == endX && y == endY) {
                    thirdFloorGrid.set(x, y, START_ROOM)
                    floorRooms[2].set(x, y, ROOM_CORRIDOR_FLAG)
                }
            }
        }

        val corridors = HORIZONTAL.filter { thirdFloorGrid[endX + it.dx, endY + it.dz] == CLEAR }
        if (corridors.isEmpty()) {
            thirdFloorGrid.set(0, 0, thirdFloorGrid.width, thirdFloorGrid.height, BLOCKED)
            floor.set(roomX, roomY, data)
            return
        }
        val corridor = corridors[random.nextInt(corridors.size)]
        recursiveCorridor(thirdFloorGrid, endX + corridor.dx, endY + corridor.dz, corridor, 4)

        @Suppress("ControlFlowWithEmptyBody")
        while (cleanEdges(thirdFloorGrid)) {
        }
    }

    /**
     * Groups the free cells into rooms of 1x1, 1x2 or 2x2 and gives each an id and a door. The
     * shuffle is what makes two mansions differ, so it has to be vanilla's own.
     */
    private fun identifyRooms(
        fromGrid: SimpleGrid,
        roomGrid: SimpleGrid,
    ) {
        val positions = mutableListOf<Pair<Int, Int>>()
        for (y in 0 until fromGrid.height) {
            for (x in 0 until fromGrid.width) {
                if (fromGrid[x, y] == ROOM) positions += x to y
            }
        }
        shuffle(positions)
        var roomId = 10

        for ((x, y) in positions) {
            if (roomGrid[x, y] != CLEAR) continue
            var x0 = x
            var x1 = x
            var y0 = y
            var y1 = y
            var type = ROOM_1X1
            if (roomGrid[x + 1, y] == CLEAR &&
                roomGrid[x, y + 1] == CLEAR &&
                roomGrid[x + 1, y + 1] == CLEAR &&
                fromGrid[x + 1, y] == ROOM &&
                fromGrid[x, y + 1] == ROOM &&
                fromGrid[x + 1, y + 1] == ROOM
            ) {
                x1++
                y1++
                type = ROOM_2X2
            } else if (roomGrid[x - 1, y] == CLEAR &&
                roomGrid[x, y + 1] == CLEAR &&
                roomGrid[x - 1, y + 1] == CLEAR &&
                fromGrid[x - 1, y] == ROOM &&
                fromGrid[x, y + 1] == ROOM &&
                fromGrid[x - 1, y + 1] == ROOM
            ) {
                x0--
                y1++
                type = ROOM_2X2
            } else if (roomGrid[x - 1, y] == CLEAR &&
                roomGrid[x, y - 1] == CLEAR &&
                roomGrid[x - 1, y - 1] == CLEAR &&
                fromGrid[x - 1, y] == ROOM &&
                fromGrid[x, y - 1] == ROOM &&
                fromGrid[x - 1, y - 1] == ROOM
            ) {
                x0--
                y0--
                type = ROOM_2X2
            } else if (roomGrid[x + 1, y] == CLEAR && fromGrid[x + 1, y] == ROOM) {
                x1++
                type = ROOM_1X2
            } else if (roomGrid[x, y + 1] == CLEAR && fromGrid[x, y + 1] == ROOM) {
                y1++
                type = ROOM_1X2
            } else if (roomGrid[x - 1, y] == CLEAR && fromGrid[x - 1, y] == ROOM) {
                x0--
                type = ROOM_1X2
            } else if (roomGrid[x, y - 1] == CLEAR && fromGrid[x, y - 1] == ROOM) {
                y0--
                type = ROOM_1X2
            }

            var doorX = if (random.nextBoolean()) x0 else x1
            var doorY = if (random.nextBoolean()) y0 else y1
            var doorFlag = ROOM_DOOR_FLAG
            // Four corners tried in a fixed dance, and the door is dropped only when none of them
            // touches a corridor.
            if (!fromGrid.edgesTo(doorX, doorY, CORRIDOR)) {
                doorX = if (doorX == x0) x1 else x0
                doorY = if (doorY == y0) y1 else y0
                if (!fromGrid.edgesTo(doorX, doorY, CORRIDOR)) {
                    doorY = if (doorY == y0) y1 else y0
                    if (!fromGrid.edgesTo(doorX, doorY, CORRIDOR)) {
                        doorX = if (doorX == x0) x1 else x0
                        doorY = if (doorY == y0) y1 else y0
                        if (!fromGrid.edgesTo(doorX, doorY, CORRIDOR)) {
                            doorFlag = 0
                            doorX = x0
                            doorY = y0
                        }
                    }
                }
            }

            for (ry in y0..y1) {
                for (rx in x0..x1) {
                    if (rx == doorX && ry == doorY) {
                        roomGrid.set(rx, ry, ROOM_ORIGIN_FLAG or doorFlag or type or roomId)
                    } else {
                        roomGrid.set(rx, ry, type or roomId)
                    }
                }
            }
            roomId++
        }
    }

    /** // parity: Util.shuffle, which walks from the end and swaps into the tail. */
    private fun shuffle(list: MutableList<Pair<Int, Int>>) {
        for (i in list.size downTo 2) {
            val swapTo = random.nextInt(i)
            val held = list[i - 1]
            list[i - 1] = list[swapTo]
            list[swapTo] = held
        }
    }

    companion object {
        const val SIZE: Int = 11
        const val ENTRANCE_X: Int = 7
        const val ENTRANCE_Y: Int = 4

        const val CLEAR: Int = 0
        const val CORRIDOR: Int = 1
        const val ROOM: Int = 2
        const val START_ROOM: Int = 3
        const val BLOCKED: Int = 5

        const val ROOM_1X1: Int = 65536
        const val ROOM_1X2: Int = 131072
        const val ROOM_2X2: Int = 262144
        const val ROOM_ORIGIN_FLAG: Int = 1048576
        const val ROOM_DOOR_FLAG: Int = 2097152
        const val ROOM_STAIRS_FLAG: Int = 4194304
        const val ROOM_CORRIDOR_FLAG: Int = 8388608
        const val ROOM_TYPE_MASK: Int = 983040
        const val ROOM_ID_MASK: Int = 65535

        /** // parity: Direction.Plane.HORIZONTAL, north, east, south, west in that order. */
        val HORIZONTAL: List<StructureDirection> =
            listOf(
                StructureDirection.NORTH,
                StructureDirection.EAST,
                StructureDirection.SOUTH,
                StructureDirection.WEST,
            )

        /** // parity: Direction.from2DDataValue, which is south, west, north, east. */
        val BY_2D: List<StructureDirection> =
            listOf(
                StructureDirection.SOUTH,
                StructureDirection.WEST,
                StructureDirection.NORTH,
                StructureDirection.EAST,
            )

        fun isHouse(
            grid: SimpleGrid,
            x: Int,
            y: Int,
        ): Boolean = grid[x, y].let { it == CORRIDOR || it == ROOM || it == START_ROOM || it == 4 }

        fun clockWise(direction: StructureDirection): StructureDirection =
            when (direction) {
                StructureDirection.NORTH -> StructureDirection.EAST
                StructureDirection.EAST -> StructureDirection.SOUTH
                StructureDirection.SOUTH -> StructureDirection.WEST
                else -> StructureDirection.NORTH
            }

        fun counterClockWise(direction: StructureDirection): StructureDirection =
            when (direction) {
                StructureDirection.NORTH -> StructureDirection.WEST
                StructureDirection.WEST -> StructureDirection.SOUTH
                StructureDirection.SOUTH -> StructureDirection.EAST
                else -> StructureDirection.NORTH
            }
    }
}
