package org.basaltmc.server.world.terrain

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.structure.StructureBoundingBox
import org.basaltmc.server.world.structure.StructureDirection
import org.basaltmc.server.world.terrain.NetherFortressPieces.Piece
import org.basaltmc.server.world.terrain.NetherFortressPieces.replaceableByStructures
import org.basaltmc.server.world.terrain.NetherFortressPieces.stateId

/** // parity: NetherFortressPieces shared per-instance geometry and block helpers. */
internal fun Piece.fence(vararg connections: StructureDirection): Int {
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

internal fun Piece.stairs(facing: StructureDirection): Int =
    stateId(
        "nether_brick_stairs",
        linkedMapOf(
            "facing" to transformDirection(facing).name.lowercase(),
            "half" to "bottom",
            "shape" to "straight",
            "waterlogged" to "false",
        ),
    )

internal fun Piece.transformDirection(direction: StructureDirection): StructureDirection {
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
        transformed = transformed.rotate(org.basaltmc.server.world.structure.StructureRotation.CLOCKWISE_90)
    }
    return transformed
}

internal fun Piece.box(
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

internal fun Piece.placeBlock(
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
            org.basaltmc.server.world.structure
                .StructurePos(worldX, worldY, worldZ),
        )
    ) {
        chunk.setBlockStateId(worldX and 15, worldY, worldZ and 15, stateId)
    }
}

internal fun Piece.fillDown(
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
            org.basaltmc.server.world.structure
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

internal fun Piece.isInside(
    x: Int,
    y: Int,
    z: Int,
    clip: StructureBoundingBox,
): Boolean =
    clip.isInside(
        org.basaltmc.server.world.structure
            .StructurePos(worldX(x, z), boundingBox.minY + y, worldZ(x, z)),
    )

internal fun Piece.blockAt(
    chunk: Chunk,
    x: Int,
    y: Int,
    z: Int,
): Int = chunk.getBlockStateId(worldX(x, z) and 15, boundingBox.minY + y, worldZ(x, z) and 15)

internal fun Piece.isChestAt(
    chunk: Chunk,
    x: Int,
    y: Int,
    z: Int,
): Boolean = Blocks.byStateId(blockAt(chunk, x, y, z))?.block?.path == "chest"

internal fun Piece.worldX(
    x: Int,
    z: Int,
): Int =
    when (orientation) {
        StructureDirection.NORTH, StructureDirection.SOUTH -> boundingBox.minX + x
        StructureDirection.WEST -> boundingBox.maxX - z
        StructureDirection.EAST -> boundingBox.minX + z
        else -> x
    }

internal fun Piece.worldZ(
    x: Int,
    z: Int,
): Int =
    when (orientation) {
        StructureDirection.NORTH -> boundingBox.maxZ - z
        StructureDirection.SOUTH -> boundingBox.minZ + z
        StructureDirection.WEST, StructureDirection.EAST -> boundingBox.minZ + x
        else -> z
    }
