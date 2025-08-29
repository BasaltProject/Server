package org.basaltmc.server.world.structure

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk

/**
 * // parity: the block-writing half of net.minecraft.world.level.levelgen.structure.StructurePiece.
 *
 * A procedural piece is written in its own coordinates, with x running across it, z along it and y
 * up from its floor, and this turns those into world ones for the orientation it was placed with.
 * Every write is clipped to [clip], which is how vanilla lets one piece span several chunks and
 * still be written once per chunk.
 *
 * Templates do not come through here: they carry their own blocks and go through
 * [StructureTemplate]. This is for the pieces that are shaped in code, like a mineshaft's.
 */
public class StructurePieceFill(
    private val box: StructureBoundingBox,
    private val orientation: StructureDirection?,
    private val chunk: Chunk,
    private val clip: StructureBoundingBox,
    /** `OCEAN_FLOOR_WG` at a world column, which is what tells a piece it is underground. */
    private val oceanFloorHeight: (x: Int, z: Int) -> Int,
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
        chunk.setDecorationBlockStateId(worldX, worldY, worldZ, stateId, expectedExternalStateId = existing)
    }

    /** Whether the block above this one is still below the surface, so the piece is buried. */
    public fun isInterior(
        x: Int,
        y: Int,
        z: Int,
    ): Boolean {
        val worldX = worldX(x, z)
        val worldY = worldY(y + 1)
        val worldZ = worldZ(x, z)
        if (!clip.isInside(StructurePos(worldX, worldY, worldZ))) return false
        return worldY < oceanFloorHeight(worldX, worldZ)
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
