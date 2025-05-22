package org.basaltmc.world.structure

/** // parity: net.minecraft.world.level.levelgen.structure.BoundingBox, axis-aligned and inclusive on every bound. */
public data class StructureBoundingBox(
    val minX: Int,
    val minY: Int,
    val minZ: Int,
    val maxX: Int,
    val maxY: Int,
    val maxZ: Int,
) {
    public fun isInside(pos: StructurePos): Boolean = pos.x in minX..maxX && pos.y in minY..maxY && pos.z in minZ..maxZ

    public fun move(offset: StructurePos): StructureBoundingBox =
        StructureBoundingBox(
            minX + offset.x,
            minY + offset.y,
            minZ + offset.z,
            maxX + offset.x,
            maxY + offset.y,
            maxZ + offset.z,
        )

    public fun intersects(other: StructureBoundingBox): Boolean =
        minX <= other.maxX &&
            maxX >= other.minX &&
            minZ <= other.maxZ &&
            maxZ >= other.minZ &&
            minY <= other.maxY &&
            maxY >= other.minY

    public companion object {
        public fun fromCorners(
            corner1: StructurePos,
            corner2: StructurePos,
        ): StructureBoundingBox =
            StructureBoundingBox(
                minOf(corner1.x, corner2.x),
                minOf(corner1.y, corner2.y),
                minOf(corner1.z, corner2.z),
                maxOf(corner1.x, corner2.x),
                maxOf(corner1.y, corner2.y),
                maxOf(corner1.z, corner2.z),
            )
    }
}
