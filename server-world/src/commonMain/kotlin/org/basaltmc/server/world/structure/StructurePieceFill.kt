package org.basaltmc.server.world.structure

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.ChunkGeneratedEntity
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound

/**
 * // parity: the block-writing half of `StructurePiece`. A procedural piece is written in its own
 * coordinates and turned into world ones; every write is clipped to [clip], which is how one piece
 * spans several chunks and is still written once per chunk. Templates go through [StructureTemplate].
 */
public class StructurePieceFill(
    private val box: StructureBoundingBox,
    private val orientation: StructureDirection?,
    private val chunk: Chunk,
    private val clip: StructureBoundingBox,
    /** A piece may refuse to overwrite what is already there; by default it does not. */
    private val canBeReplaced: (stateId: Int) -> Boolean = { true },
) {
    public fun worldX(
        x: Int,
        z: Int,
    ): Int =
        when (orientation) {
            null -> x
            StructureDirection.NORTH, StructureDirection.SOUTH -> box.minX + x
            StructureDirection.WEST -> box.maxX - z
            StructureDirection.EAST -> box.minX + z
            else -> x
        }

    public fun worldY(y: Int): Int = if (orientation == null) y else y + box.minY

    public fun worldZ(
        x: Int,
        z: Int,
    ): Int =
        when (orientation) {
            null -> z
            StructureDirection.NORTH -> box.maxZ - z
            StructureDirection.SOUTH -> box.minZ + z
            StructureDirection.WEST, StructureDirection.EAST -> box.minZ + x
            else -> z
        }

    /** The block already at a piece-local position, or air when it falls outside [clip]. */
    public fun blockAt(
        x: Int,
        y: Int,
        z: Int,
    ): Int {
        val worldX = worldX(x, z)
        val worldY = worldY(y)
        val worldZ = worldZ(x, z)
        if (!clip.isInside(StructurePos(worldX, worldY, worldZ))) return Blocks.AIR.stateId
        return chunk.getDecorationBlockStateId(worldX, worldY, worldZ)
    }

    public fun place(
        stateId: Int,
        x: Int,
        y: Int,
        z: Int,
    ) {
        val worldX = worldX(x, z)
        val worldY = worldY(y)
        val worldZ = worldZ(x, z)
        if (!clip.isInside(StructurePos(worldX, worldY, worldZ))) return
        val existing = chunk.getDecorationBlockStateId(worldX, worldY, worldZ)
        if (!canBeReplaced(existing)) return
        chunk.setDecorationBlockStateId(worldX, worldY, worldZ, oriented(stateId), expectedExternalStateId = existing)
    }

    /**
     * // parity: `StructurePiece.placeBlock` turns the state by the piece's orientation, exactly as it
     * turns the coordinates: a piece is written facing north and then rotated whole.
     */
    private fun oriented(stateId: Int): Int {
        if (orientation == null || orientation == StructureDirection.NORTH) return stateId
        val properties = Blocks.byStateId(stateId)?.properties ?: return stateId
        if (properties.isEmpty()) return stateId
        return TEMPLATE.transformedStateId(stateId, properties, mirror, rotation)
    }

    /** // parity: the mirror and rotation `setOrientation` derives from a piece's facing. */
    private val mirror =
        when (orientation) {
            StructureDirection.SOUTH, StructureDirection.WEST -> StructureMirror.LEFT_RIGHT
            else -> StructureMirror.NONE
        }

    private val rotation =
        when (orientation) {
            StructureDirection.WEST, StructureDirection.EAST -> StructureRotation.CLOCKWISE_90
            else -> StructureRotation.NONE
        }

    private companion object {
        val TEMPLATE = StructureTemplate()
    }

    /**
     * Adds a generated entity at the centre of a piece-local block, if this chunk owns it.
     * // parity: the structures that call `addFreshEntity` while placing, such as the mineshaft's
     * chest minecart, which rides the rail it just put down.
     */
    public fun placeEntity(
        type: Identifier,
        x: Int,
        y: Int,
        z: Int,
        data: NbtCompound,
        /** Some structures stand an entity on the block instead of inside it. */
        yOffset: Double = 0.5,
    ) {
        val worldX = worldX(x, z)
        val worldY = worldY(y)
        val worldZ = worldZ(x, z)
        if (!clip.isInside(StructurePos(worldX, worldY, worldZ))) return
        chunk.addGeneratedEntity(
            ChunkGeneratedEntity(
                type = type,
                x = worldX + 0.5,
                y = worldY + yOffset,
                z = worldZ + 0.5,
                yaw = 0.0f,
                pitch = 0.0f,
                data = data,
            ),
        )
    }

    /**
     * Whether the block above is still below the surface, so the piece is buried. // parity:
     * StructurePiece.isInterior reads `OCEAN_FLOOR_WG`; the chunk's own frozen map answers it, where a
     * column function would give a lower floor wherever a structure raised the ground.
     */
    public fun isInterior(
        x: Int,
        y: Int,
        z: Int,
    ): Boolean {
        val worldX = worldX(x, z)
        val worldY = worldY(y + 1)
        val worldZ = worldZ(x, z)
        if (!clip.isInside(StructurePos(worldX, worldY, worldZ))) return false
        return worldY < chunk.worldgenHeightAt(worldX and 15, worldZ and 15, Chunk.HeightmapType.OCEAN_FLOOR)
    }

    /**
     * The shell of the box in [edge] and its inside in [fill]. With [skipAir] the cells that are
     * already air are left alone, which is how a piece avoids walling itself into a cave.
     */
    public fun generateBox(
        x0: Int,
        y0: Int,
        z0: Int,
        x1: Int,
        y1: Int,
        z1: Int,
        edge: Int,
        fill: Int,
        skipAir: Boolean,
    ) {
        for (y in y0..y1) {
            for (x in x0..x1) {
                for (z in z0..z1) {
                    if (skipAir && Blocks.isAir(blockAt(x, y, z))) continue
                    val inside = y != y0 && y != y1 && x != x0 && x != x1 && z != z0 && z != z1
                    place(if (inside) fill else edge, x, y, z)
                }
            }
        }
    }

    /**
     * [generateBox] with a roll per cell. // parity: the roll happens for every cell before the
     * other two conditions are even looked at, so the stream does not depend on the terrain.
     */
    public fun generateMaybeBox(
        random: BasaltRandom,
        probability: Float,
        x0: Int,
        y0: Int,
        z0: Int,
        x1: Int,
        y1: Int,
        z1: Int,
        edge: Int,
        fill: Int,
        skipAir: Boolean,
        onlyInside: Boolean,
    ) {
        for (y in y0..y1) {
            for (x in x0..x1) {
                for (z in z0..z1) {
                    val roll = random.nextFloat()
                    if (roll > probability) continue
                    if (skipAir && Blocks.isAir(blockAt(x, y, z))) continue
                    if (onlyInside && !isInterior(x, y, z)) continue
                    val inside = y != y0 && y != y1 && x != x0 && x != x1 && z != z0 && z != z1
                    place(if (inside) fill else edge, x, y, z)
                }
            }
        }
    }

    /**
     * // parity: generateUpperHalfSphere. The dome a mineshaft room is roofed with: an ellipsoid
     * measured from the floor up, so only its top half lands inside the box.
     */
    public fun generateUpperHalfSphere(
        x0: Int,
        y0: Int,
        z0: Int,
        x1: Int,
        y1: Int,
        z1: Int,
        fill: Int,
        skipAir: Boolean,
    ) {
        val diagX = (x1 - x0 + 1).toFloat()
        val diagY = (y1 - y0 + 1).toFloat()
        val diagZ = (z1 - z0 + 1).toFloat()
        val centreX = x0 + diagX / 2.0f
        val centreZ = z0 + diagZ / 2.0f
        for (y in y0..y1) {
            val dy = (y - y0) / diagY
            for (x in x0..x1) {
                val dx = (x - centreX) / (diagX * 0.5f)
                for (z in z0..z1) {
                    val dz = (z - centreZ) / (diagZ * 0.5f)
                    if (skipAir && Blocks.isAir(blockAt(x, y, z))) continue
                    if (dx * dx + dy * dy + dz * dz <= 1.05f) place(fill, x, y, z)
                }
            }
        }
    }

    public fun maybePlace(
        random: BasaltRandom,
        probability: Float,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
    ) {
        if (random.nextFloat() < probability) place(stateId, x, y, z)
    }
}
