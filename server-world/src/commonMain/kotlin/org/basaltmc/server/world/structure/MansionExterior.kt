package org.basaltmc.server.world.structure

/** One template of a mansion, ready to be placed. // parity: WoodlandMansionPiece. */
internal data class MansionPiece(
    val template: String,
    val position: StructurePos,
    val rotation: StructureRotation,
    val mirror: StructureMirror = StructureMirror.NONE,
)

/** // parity: WoodlandMansionPieces.PlacementData, the cursor the wall walk carries around. */
internal class MansionPlacement(
    var position: StructurePos,
    var rotation: StructureRotation,
    var wallType: String,
)

/**
 * // parity: the wall and roof half of WoodlandMansionPieces.MansionPiecePlacer. The walk goes round
 * the outline one cell at a time, turning outward at a corner and inward at a notch. [startX] and
 * [startY] are the grid cell the house is measured from, just inside the entrance.
 */
internal class MansionExterior(
    private val startX: Int,
    private val startY: Int,
) {
    fun entrance(
        pieces: MutableList<MansionPiece>,
        data: MansionPlacement,
    ) {
        val west = data.rotation.rotate(StructureDirection.WEST)
        pieces += MansionPiece("entrance", data.position.relative(west, 9), data.rotation)
        data.position = data.position.relative(data.rotation.rotate(StructureDirection.SOUTH), 16)
    }

    fun wallPiece(
        pieces: MutableList<MansionPiece>,
        data: MansionPlacement,
    ) {
        pieces +=
            MansionPiece(
                data.wallType,
                data.position.relative(data.rotation.rotate(StructureDirection.EAST), 7),
                data.rotation,
            )
        data.position = data.position.relative(data.rotation.rotate(StructureDirection.SOUTH), 8)
    }

    /** An outward corner: a corner piece, then the cursor swings a quarter turn clockwise. */
    fun turn(
        pieces: MutableList<MansionPiece>,
        data: MansionPlacement,
    ) {
        data.position = data.position.relative(data.rotation.rotate(StructureDirection.SOUTH), -1)
        pieces += MansionPiece("wall_corner", data.position, data.rotation)
        data.position = data.position.relative(data.rotation.rotate(StructureDirection.SOUTH), -7)
        data.position = data.position.relative(data.rotation.rotate(StructureDirection.WEST), -6)
        data.rotation = data.rotation.rotated(StructureRotation.CLOCKWISE_90)
    }

    /** An inward corner, which places nothing: the wall just folds around the notch. */
    fun innerTurn(data: MansionPlacement) {
        data.position = data.position.relative(data.rotation.rotate(StructureDirection.SOUTH), 6)
        data.position = data.position.relative(data.rotation.rotate(StructureDirection.EAST), 8)
        data.rotation = data.rotation.rotated(StructureRotation.COUNTERCLOCKWISE_90)
    }

    /**
     * Walks the outline from ([startGridX], [startGridY]) until it comes back to ([endX], [endY])
     * heading the way it set out. // parity: traverseOuterWalls, whose loop is a do-while, so a
     * one-cell house still lays its four corners.
     */
    fun traverseOuterWalls(
        pieces: MutableList<MansionPiece>,
        data: MansionPlacement,
        grid: SimpleGrid,
        heading: StructureDirection,
        startGridX: Int,
        startGridY: Int,
        endX: Int,
        endY: Int,
    ) {
        var gridX = startGridX
        var gridY = startGridY
        var direction = heading
        val startDirection = heading
        do {
            val aheadIsHouse = MansionGrid.isHouse(grid, gridX + direction.dx, gridY + direction.dz)
            val ccw = MansionGrid.counterClockWise(direction)
            when {
                !aheadIsHouse -> {
                    turn(pieces, data)
                    direction = MansionGrid.clockWise(direction)
                    if (gridX != endX || gridY != endY || startDirection != direction) wallPiece(pieces, data)
                }
                MansionGrid.isHouse(grid, gridX + direction.dx + ccw.dx, gridY + direction.dz + ccw.dz) -> {
                    innerTurn(data)
                    gridX += direction.dx
                    gridY += direction.dz
                    direction = ccw
                }
                else -> {
                    gridX += direction.dx
                    gridY += direction.dz
                    if (gridX != endX || gridY != endY || startDirection != direction) wallPiece(pieces, data)
                }
            }
        } while (gridX != endX || gridY != endY || startDirection != direction)
    }

    /**
     * // parity: createRoof. Three passes over the grid: the roof slabs and their fronts, then the
     * short walls where a floor above continues, then the corners. They are separate passes and not
     * one, so the piece order in the list is not cell by cell.
     */
    fun createRoof(
        pieces: MutableList<MansionPiece>,
        roofOrigin: StructurePos,
        rotation: StructureRotation,
        grid: SimpleGrid,
        aboveGrid: SimpleGrid?,
    ) {
        forEachCell(grid) { x, y ->
            val position = cellPosition(roofOrigin, rotation, x, y)
            val isAbove = aboveGrid != null && MansionGrid.isHouse(aboveGrid, x, y)
            if (!MansionGrid.isHouse(grid, x, y) || isAbove) return@forEachCell
            pieces += MansionPiece("roof", position.above(3), rotation)
            if (!MansionGrid.isHouse(grid, x + 1, y)) {
                pieces +=
                    MansionPiece("roof_front", position.relative(rotation.rotate(StructureDirection.EAST), 6), rotation)
            }
            if (!MansionGrid.isHouse(grid, x - 1, y)) {
                val p = position.relative(rotation.rotate(StructureDirection.SOUTH), 7)
                pieces += MansionPiece("roof_front", p, rotation.rotated(StructureRotation.CLOCKWISE_180))
            }
            if (!MansionGrid.isHouse(grid, x, y - 1)) {
                val p = position.relative(rotation.rotate(StructureDirection.WEST), 1)
                pieces += MansionPiece("roof_front", p, rotation.rotated(StructureRotation.COUNTERCLOCKWISE_90))
            }
            if (!MansionGrid.isHouse(grid, x, y + 1)) {
                var p = position.relative(rotation.rotate(StructureDirection.EAST), 6)
                p = p.relative(rotation.rotate(StructureDirection.SOUTH), 6)
                pieces += MansionPiece("roof_front", p, rotation.rotated(StructureRotation.CLOCKWISE_90))
            }
        }

        if (aboveGrid != null) {
            forEachCell(grid) { x, y ->
                val position = cellPosition(roofOrigin, rotation, x, y)
                if (!MansionGrid.isHouse(grid, x, y) || !MansionGrid.isHouse(aboveGrid, x, y)) return@forEachCell
                smallWalls(pieces, position, rotation, grid, x, y)
            }
        }

        forEachCell(grid) { x, y ->
            val position = cellPosition(roofOrigin, rotation, x, y)
            val isAbove = aboveGrid != null && MansionGrid.isHouse(aboveGrid, x, y)
            if (!MansionGrid.isHouse(grid, x, y) || isAbove) return@forEachCell
            roofCorners(pieces, position, rotation, grid, x, y)
        }
    }

    private fun smallWalls(
        pieces: MutableList<MansionPiece>,
        position: StructurePos,
        rotation: StructureRotation,
        grid: SimpleGrid,
        x: Int,
        y: Int,
    ) {
        val east = rotation.rotate(StructureDirection.EAST)
        val west = rotation.rotate(StructureDirection.WEST)
        val north = rotation.rotate(StructureDirection.NORTH)
        val south = rotation.rotate(StructureDirection.SOUTH)
        if (!MansionGrid.isHouse(grid, x + 1, y)) {
            pieces += MansionPiece("small_wall", position.relative(east, 7), rotation)
        }
        if (!MansionGrid.isHouse(grid, x - 1, y)) {
            val p = position.relative(west, 1).relative(south, 6)
            pieces += MansionPiece("small_wall", p, rotation.rotated(StructureRotation.CLOCKWISE_180))
        }
        if (!MansionGrid.isHouse(grid, x, y - 1)) {
            val p = position.relative(west, 0).relative(north, 1)
            pieces += MansionPiece("small_wall", p, rotation.rotated(StructureRotation.COUNTERCLOCKWISE_90))
        }
        if (!MansionGrid.isHouse(grid, x, y + 1)) {
            val p = position.relative(east, 6).relative(south, 7)
            pieces += MansionPiece("small_wall", p, rotation.rotated(StructureRotation.CLOCKWISE_90))
        }
        if (!MansionGrid.isHouse(grid, x + 1, y)) {
            if (!MansionGrid.isHouse(grid, x, y - 1)) {
                val p = position.relative(east, 7).relative(north, 2)
                pieces += MansionPiece("small_wall_corner", p, rotation)
            }
            if (!MansionGrid.isHouse(grid, x, y + 1)) {
                val p = position.relative(east, 8).relative(south, 7)
                pieces += MansionPiece("small_wall_corner", p, rotation.rotated(StructureRotation.CLOCKWISE_90))
            }
        }
        if (!MansionGrid.isHouse(grid, x - 1, y)) {
            if (!MansionGrid.isHouse(grid, x, y - 1)) {
                val p = position.relative(west, 2).relative(north, 1)
                pieces += MansionPiece("small_wall_corner", p, rotation.rotated(StructureRotation.COUNTERCLOCKWISE_90))
            }
            if (!MansionGrid.isHouse(grid, x, y + 1)) {
                val p = position.relative(west, 1).relative(south, 8)
                pieces += MansionPiece("small_wall_corner", p, rotation.rotated(StructureRotation.CLOCKWISE_180))
            }
        }
    }

    private fun roofCorners(
        pieces: MutableList<MansionPiece>,
        position: StructurePos,
        rotation: StructureRotation,
        grid: SimpleGrid,
        x: Int,
        y: Int,
    ) {
        val east = rotation.rotate(StructureDirection.EAST)
        val west = rotation.rotate(StructureDirection.WEST)
        val north = rotation.rotate(StructureDirection.NORTH)
        val south = rotation.rotate(StructureDirection.SOUTH)
        if (!MansionGrid.isHouse(grid, x + 1, y)) {
            val corner = position.relative(east, 6)
            if (!MansionGrid.isHouse(grid, x, y + 1)) {
                pieces += MansionPiece("roof_corner", corner.relative(south, 6), rotation)
            } else if (MansionGrid.isHouse(grid, x + 1, y + 1)) {
                pieces += MansionPiece("roof_inner_corner", corner.relative(south, 5), rotation)
            }
            if (!MansionGrid.isHouse(grid, x, y - 1)) {
                pieces += MansionPiece("roof_corner", corner, rotation.rotated(StructureRotation.COUNTERCLOCKWISE_90))
            } else if (MansionGrid.isHouse(grid, x + 1, y - 1)) {
                val p = position.relative(east, 9).relative(north, 2)
                pieces += MansionPiece("roof_inner_corner", p, rotation.rotated(StructureRotation.CLOCKWISE_90))
            }
        }
        if (!MansionGrid.isHouse(grid, x - 1, y)) {
            if (!MansionGrid.isHouse(grid, x, y + 1)) {
                pieces +=
                    MansionPiece(
                        "roof_corner",
                        position.relative(south, 6),
                        rotation.rotated(StructureRotation.CLOCKWISE_90),
                    )
            } else if (MansionGrid.isHouse(grid, x - 1, y + 1)) {
                val p = position.relative(south, 8).relative(west, 3)
                pieces += MansionPiece("roof_inner_corner", p, rotation.rotated(StructureRotation.COUNTERCLOCKWISE_90))
            }
            if (!MansionGrid.isHouse(grid, x, y - 1)) {
                pieces += MansionPiece("roof_corner", position, rotation.rotated(StructureRotation.CLOCKWISE_180))
            } else if (MansionGrid.isHouse(grid, x - 1, y - 1)) {
                val p = position.relative(south, 1)
                pieces += MansionPiece("roof_inner_corner", p, rotation.rotated(StructureRotation.CLOCKWISE_180))
            }
        }
    }

    /** Where a grid cell lands in the world, measured from the cell the house starts at. */
    fun cellPosition(
        origin: StructurePos,
        rotation: StructureRotation,
        x: Int,
        y: Int,
    ): StructurePos =
        origin
            .relative(rotation.rotate(StructureDirection.SOUTH), 8 + (y - startY) * 8)
            .relative(rotation.rotate(StructureDirection.EAST), (x - startX) * 8)

    private inline fun forEachCell(
        grid: SimpleGrid,
        action: (Int, Int) -> Unit,
    ) {
        for (y in 0 until grid.height) {
            for (x in 0 until grid.width) action(x, y)
        }
    }
}

/** // parity: Rotation.rotate(Direction). The vertical faces are left alone. */
internal fun StructureRotation.rotate(direction: StructureDirection): StructureDirection =
    when (this) {
        StructureRotation.NONE -> direction
        StructureRotation.CLOCKWISE_90 -> MansionGrid.clockWise(direction)
        StructureRotation.CLOCKWISE_180 -> direction.opposite()
        StructureRotation.COUNTERCLOCKWISE_90 -> MansionGrid.counterClockWise(direction)
    }

/** // parity: Rotation.getRotated, the composition of two quarter turns. */
internal fun StructureRotation.rotated(other: StructureRotation): StructureRotation {
    val quarters = (QUARTERS.getValue(this) + QUARTERS.getValue(other)) % 4
    return QUARTERS.entries.first { it.value == quarters }.key
}

private val QUARTERS =
    mapOf(
        StructureRotation.NONE to 0,
        StructureRotation.CLOCKWISE_90 to 1,
        StructureRotation.CLOCKWISE_180 to 2,
        StructureRotation.COUNTERCLOCKWISE_90 to 3,
    )

internal fun StructurePos.relative(
    direction: StructureDirection,
    distance: Int,
): StructurePos = StructurePos(x + direction.dx * distance, y + direction.dy * distance, z + direction.dz * distance)

internal fun StructurePos.above(distance: Int): StructurePos = StructurePos(x, y + distance, z)

internal fun StructurePos.offset(
    dx: Int,
    dy: Int,
    dz: Int,
): StructurePos = StructurePos(x + dx, y + dy, z + dz)

/** // parity: BlockPos.rotate, a turn about the origin. */
internal fun StructurePos.rotate(rotation: StructureRotation): StructurePos =
    when (rotation) {
        StructureRotation.NONE -> this
        StructureRotation.CLOCKWISE_90 -> StructurePos(-z, y, x)
        StructureRotation.CLOCKWISE_180 -> StructurePos(-x, y, -z)
        StructureRotation.COUNTERCLOCKWISE_90 -> StructurePos(z, y, -x)
    }

/** // parity: StructureTemplate.getZeroPositionWithTransform, which is where a turned template's own zero lands. */
internal fun zeroPositionWithTransform(
    zeroPos: StructurePos,
    mirror: StructureMirror,
    rotation: StructureRotation,
    width: Int,
    depth: Int,
): StructurePos {
    val sizeX = width - 1
    val sizeZ = depth - 1
    val mirrorDeltaX = if (mirror == StructureMirror.FRONT_BACK) sizeX else 0
    val mirrorDeltaZ = if (mirror == StructureMirror.LEFT_RIGHT) sizeZ else 0
    return when (rotation) {
        StructureRotation.COUNTERCLOCKWISE_90 -> zeroPos.offset(mirrorDeltaZ, 0, sizeX - mirrorDeltaX)
        StructureRotation.CLOCKWISE_90 -> zeroPos.offset(sizeZ - mirrorDeltaZ, 0, mirrorDeltaX)
        StructureRotation.CLOCKWISE_180 -> zeroPos.offset(sizeX - mirrorDeltaX, 0, sizeZ - mirrorDeltaZ)
        StructureRotation.NONE -> zeroPos.offset(mirrorDeltaX, 0, mirrorDeltaZ)
    }
}
