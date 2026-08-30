package org.basaltmc.server.world.structure

import org.basaltmc.server.random.BasaltRandom

/**
 * What fills a room once its walls are up. // parity: the three `FloorRoomCollection`s, kept behind
 * an interface so the walk that decides *where* a room goes does not have to know the catalogue of
 * what goes in it.
 */
internal interface MansionRooms {
    fun addRoom1X1(
        pieces: MutableList<MansionPiece>,
        position: StructurePos,
        rotation: StructureRotation,
        doorDirection: StructureDirection?,
        floor: Int,
    )

    fun addRoom1X2(
        pieces: MutableList<MansionPiece>,
        position: StructurePos,
        rotation: StructureRotation,
        roomDirection: StructureDirection?,
        doorDirection: StructureDirection,
        floor: Int,
        isStairs: Boolean,
    )

    fun addRoom2X2(
        pieces: MutableList<MansionPiece>,
        position: StructurePos,
        rotation: StructureRotation,
        roomDirection: StructureDirection,
        doorDirection: StructureDirection,
        floor: Int,
    )

    fun addRoom2X2Secret(
        pieces: MutableList<MansionPiece>,
        position: StructurePos,
        rotation: StructureRotation,
        floor: Int,
    )
}

/**
 * // parity: WoodlandMansionPieces.MansionPiecePlacer.createMansion. Turns a floor plan into the
 * list of templates that make the house: the outer walls of the three storeys, the roofs, then per
 * floor the corridor tiles with their carpets and the walls and doors that close each room.
 */
internal class MansionPlacer(
    private val random: BasaltRandom,
    private val rooms: MansionRooms,
) {
    fun createMansion(
        origin: StructurePos,
        rotation: StructureRotation,
        pieces: MutableList<MansionPiece>,
        mansion: MansionGrid,
    ) {
        val startX = MansionGrid.ENTRANCE_X + 1
        val startY = MansionGrid.ENTRANCE_Y + 1
        val exterior = MansionExterior(startX, startY)

        val data = MansionPlacement(origin, rotation, "wall_flat")
        exterior.entrance(pieces, data)
        val secondData = MansionPlacement(data.position.above(8), data.rotation, "wall_window")

        val baseGrid = mansion.baseGrid
        val thirdGrid = mansion.thirdFloorGrid
        val endX = MansionGrid.ENTRANCE_X + 1
        val endY = MansionGrid.ENTRANCE_Y
        exterior.traverseOuterWalls(pieces, data, baseGrid, StructureDirection.SOUTH, startX, startY, endX, endY)
        exterior.traverseOuterWalls(pieces, secondData, baseGrid, StructureDirection.SOUTH, startX, startY, endX, endY)

        // The tower: its wall starts at the first house cell found scanning rows from the north and
        // columns from the east, which is not the reading order and decides where its wall begins.
        val thirdData = MansionPlacement(data.position.above(19), data.rotation, "wall_window")
        var done = false
        for (y in 0 until thirdGrid.height) {
            if (done) break
            for (x in thirdGrid.width - 1 downTo 0) {
                if (!MansionGrid.isHouse(thirdGrid, x, y)) continue
                thirdData.position =
                    thirdData.position
                        .relative(rotation.rotate(StructureDirection.SOUTH), 8 + (y - startY) * 8)
                        .relative(rotation.rotate(StructureDirection.EAST), (x - startX) * 8)
                exterior.wallPiece(pieces, thirdData)
                exterior.traverseOuterWalls(pieces, thirdData, thirdGrid, StructureDirection.SOUTH, x, y, x, y)
                done = true
                break
            }
        }

        exterior.createRoof(pieces, origin.above(16), rotation, baseGrid, thirdGrid)
        exterior.createRoof(pieces, origin.above(27), rotation, thirdGrid, null)

        for (floor in 0 until 3) {
            val floorOrigin = origin.above(8 * floor + if (floor == 2) 3 else 0)
            val roomGrid = mansion.floorRooms[floor]
            val grid = if (floor == 2) thirdGrid else baseGrid
            corridorFloor(pieces, floorOrigin, rotation, grid, roomGrid, floor, startX, startY)
            roomWalls(pieces, floorOrigin, rotation, mansion, grid, roomGrid, floor, startX, startY)
        }
    }

    /** The corridor tiles, each with a carpet on any side that carries on being corridor. */
    private fun corridorFloor(
        pieces: MutableList<MansionPiece>,
        floorOrigin: StructurePos,
        rotation: StructureRotation,
        grid: SimpleGrid,
        roomGrid: SimpleGrid,
        floor: Int,
        startX: Int,
        startY: Int,
    ) {
        val southPiece = if (floor == 0) "carpet_south_1" else "carpet_south_2"
        val westPiece = if (floor == 0) "carpet_west_1" else "carpet_west_2"
        val east = rotation.rotate(StructureDirection.EAST)
        val west = rotation.rotate(StructureDirection.WEST)
        val north = rotation.rotate(StructureDirection.NORTH)
        val south = rotation.rotate(StructureDirection.SOUTH)

        fun carpeted(
            x: Int,
            y: Int,
        ) = grid[x, y] == MansionGrid.CORRIDOR ||
            (roomGrid[x, y] and MansionGrid.ROOM_CORRIDOR_FLAG) == MansionGrid.ROOM_CORRIDOR_FLAG

        for (y in 0 until grid.height) {
            for (x in 0 until grid.width) {
                if (grid[x, y] != MansionGrid.CORRIDOR) continue
                val pos =
                    floorOrigin
                        .relative(south, 8 + (y - startY) * 8)
                        .relative(east, (x - startX) * 8)
                pieces += MansionPiece("corridor_floor", pos, rotation)
                if (carpeted(x, y - 1)) {
                    pieces += MansionPiece("carpet_north", pos.relative(east, 1).above(1), rotation)
                }
                if (carpeted(x + 1, y)) {
                    pieces += MansionPiece("carpet_east", pos.relative(south, 1).relative(east, 5).above(1), rotation)
                }
                if (carpeted(x, y + 1)) {
                    pieces += MansionPiece(southPiece, pos.relative(south, 5).relative(west, 1), rotation)
                }
                if (carpeted(x - 1, y)) {
                    pieces += MansionPiece(westPiece, pos.relative(west, 1).relative(north, 1), rotation)
                }
            }
        }
    }

    /** The four walls of every room, one of which becomes its door, and then whatever fills it. */
    private fun roomWalls(
        pieces: MutableList<MansionPiece>,
        floorOrigin: StructurePos,
        rotation: StructureRotation,
        mansion: MansionGrid,
        grid: SimpleGrid,
        roomGrid: SimpleGrid,
        floor: Int,
        startX: Int,
        startY: Int,
    ) {
        val wallPiece = if (floor == 0) "indoors_wall_1" else "indoors_wall_2"
        val doorPiece = if (floor == 0) "indoors_door_1" else "indoors_door_2"
        val east = rotation.rotate(StructureDirection.EAST)
        val north = rotation.rotate(StructureDirection.NORTH)
        val south = rotation.rotate(StructureDirection.SOUTH)
        val quarter = rotation.rotated(StructureRotation.CLOCKWISE_90)

        for (y in 0 until grid.height) {
            for (x in 0 until grid.width) {
                var towerRoom = floor == 2 && grid[x, y] == MansionGrid.START_ROOM
                if (grid[x, y] != MansionGrid.ROOM && !towerRoom) continue
                val roomData = roomGrid[x, y]
                val roomType = roomData and MansionGrid.ROOM_TYPE_MASK
                val roomId = roomData and MansionGrid.ROOM_ID_MASK
                towerRoom = towerRoom && (roomData and MansionGrid.ROOM_CORRIDOR_FLAG) == MansionGrid.ROOM_CORRIDOR_FLAG

                val doorDirections =
                    if ((roomData and MansionGrid.ROOM_DOOR_FLAG) == MansionGrid.ROOM_DOOR_FLAG) {
                        MansionGrid.HORIZONTAL.filter { grid[x + it.dx, y + it.dz] == MansionGrid.CORRIDOR }
                    } else {
                        emptyList()
                    }
                val doorDirection =
                    when {
                        doorDirections.isNotEmpty() -> doorDirections[random.nextInt(doorDirections.size)]
                        (roomData and MansionGrid.ROOM_ORIGIN_FLAG) == MansionGrid.ROOM_ORIGIN_FLAG ->
                            StructureDirection.UP
                        else -> null
                    }

                val roomPos =
                    floorOrigin
                        .relative(south, 8 + (y - startY) * 8)
                        .relative(east, -1 + (x - startX) * 8)

                if (MansionGrid.isHouse(grid, x - 1, y) && !mansion.isRoomId(x - 1, y, floor, roomId)) {
                    val template = if (doorDirection == StructureDirection.WEST) doorPiece else wallPiece
                    pieces += MansionPiece(template, roomPos, rotation)
                }
                if (grid[x + 1, y] == MansionGrid.CORRIDOR && !towerRoom) {
                    val template = if (doorDirection == StructureDirection.EAST) doorPiece else wallPiece
                    pieces += MansionPiece(template, roomPos.relative(east, 8), rotation)
                }
                if (MansionGrid.isHouse(grid, x, y + 1) && !mansion.isRoomId(x, y + 1, floor, roomId)) {
                    val template = if (doorDirection == StructureDirection.SOUTH) doorPiece else wallPiece
                    pieces += MansionPiece(template, roomPos.relative(south, 7).relative(east, 7), quarter)
                }
                if (grid[x, y - 1] == MansionGrid.CORRIDOR && !towerRoom) {
                    val template = if (doorDirection == StructureDirection.NORTH) doorPiece else wallPiece
                    pieces += MansionPiece(template, roomPos.relative(north, 1).relative(east, 7), quarter)
                }

                fillRoom(
                    pieces,
                    roomPos,
                    rotation,
                    mansion,
                    grid,
                    floor,
                    x,
                    y,
                    roomId,
                    roomType,
                    roomData,
                    doorDirection,
                )
            }
        }
    }

    private fun fillRoom(
        pieces: MutableList<MansionPiece>,
        roomPos: StructurePos,
        rotation: StructureRotation,
        mansion: MansionGrid,
        grid: SimpleGrid,
        floor: Int,
        x: Int,
        y: Int,
        roomId: Int,
        roomType: Int,
        roomData: Int,
        doorDirection: StructureDirection?,
    ) {
        when {
            roomType == MansionGrid.ROOM_1X1 -> rooms.addRoom1X1(pieces, roomPos, rotation, doorDirection, floor)
            roomType == MansionGrid.ROOM_1X2 && doorDirection != null -> {
                val roomDirection = mansion.roomDirection1x2(x, y, floor, roomId)
                val isStairs = (roomData and MansionGrid.ROOM_STAIRS_FLAG) == MansionGrid.ROOM_STAIRS_FLAG
                rooms.addRoom1X2(pieces, roomPos, rotation, roomDirection, doorDirection, floor, isStairs)
            }
            roomType == MansionGrid.ROOM_2X2 && doorDirection != null && doorDirection != StructureDirection.UP -> {
                // The 2x2 is entered from one of its two halves, so which way it opens is the door's
                // clockwise neighbour, or the other side when that one is not part of the same room.
                var roomDirection = MansionGrid.clockWise(doorDirection)
                if (!mansion.isRoomId(x + roomDirection.dx, y + roomDirection.dz, floor, roomId)) {
                    roomDirection = roomDirection.opposite()
                }
                rooms.addRoom2X2(pieces, roomPos, rotation, roomDirection, doorDirection, floor)
            }
            roomType == MansionGrid.ROOM_2X2 && doorDirection == StructureDirection.UP ->
                rooms.addRoom2X2Secret(pieces, roomPos, rotation, floor)
        }
    }
}
