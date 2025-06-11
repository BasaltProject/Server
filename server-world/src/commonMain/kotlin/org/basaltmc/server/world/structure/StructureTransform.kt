package org.basaltmc.server.world.structure

/** A block position in structure/world space (not chunk-relative, unlike `DecorationPos`). */
public data class StructurePos(
    val x: Int,
    val y: Int,
    val z: Int,
) {
    public operator fun plus(other: StructurePos): StructurePos = StructurePos(x + other.x, y + other.y, z + other.z)

    public operator fun minus(other: StructurePos): StructurePos = StructurePos(x - other.x, y - other.y, z - other.z)
}

/** // parity: net.minecraft.world.level.block.Rotation. */
public enum class StructureRotation {
    NONE,
    CLOCKWISE_90,
    CLOCKWISE_180,
    COUNTERCLOCKWISE_90,
    ;

    /** What `/place template` takes, which is `Rotation.getSerializedName`. */
    public val serializedName: String get() = name.lowercase()
}

/** // parity: net.minecraft.world.level.block.Mirror. */
public enum class StructureMirror {
    NONE,
    LEFT_RIGHT,
    FRONT_BACK,
    ;

    /** What `/place template` takes, which is `Mirror.getSerializedName`. */
    public val serializedName: String get() = name.lowercase()
}

/**
 * // parity: net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplate
 * .transform(BlockPos, Mirror, Rotation, BlockPos). Which enum constant maps to which case was
 * not obvious from the decompiled switch (javac lowers it to an opaque `$SwitchMap` array), so
 * this was resolved by disassembling `StructureTemplate$1`'s static initializer directly: mirror
 * `LEFT_RIGHT`/`FRONT_BACK` negate Z/X respectively (`NONE` does neither); rotation
 * `COUNTERCLOCKWISE_90`/`CLOCKWISE_90`/`CLOCKWISE_180` each have their own pivot formula, `NONE`
 * returns the (possibly mirrored) position unchanged.
 */
public object StructureTransform {
    public fun transform(
        pos: StructurePos,
        mirror: StructureMirror,
        rotation: StructureRotation,
        pivot: StructurePos,
    ): StructurePos {
        var x = pos.x
        var z = pos.z
        var wasMirrored = true
        when (mirror) {
            StructureMirror.LEFT_RIGHT -> z = -z
            StructureMirror.FRONT_BACK -> x = -x
            StructureMirror.NONE -> wasMirrored = false
        }
        val pivotX = pivot.x
        val pivotZ = pivot.z
        return when (rotation) {
            StructureRotation.COUNTERCLOCKWISE_90 -> StructurePos(pivotX - pivotZ + z, pos.y, pivotX + pivotZ - x)
            StructureRotation.CLOCKWISE_90 -> StructurePos(pivotX + pivotZ - z, pos.y, pivotZ - pivotX + x)
            StructureRotation.CLOCKWISE_180 -> StructurePos(pivotX + pivotX - x, pos.y, pivotZ + pivotZ - z)
            StructureRotation.NONE -> if (wasMirrored) StructurePos(x, pos.y, z) else pos
        }
    }
}
