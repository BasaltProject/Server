package org.basaltmc.server.world.structure

/** // parity: net.minecraft.core.Direction, the six axis directions a jigsaw block can face. */
public enum class StructureDirection(
    public val dx: Int,
    public val dy: Int,
    public val dz: Int,
) {
    DOWN(0, -1, 0),
    UP(0, 1, 0),
    NORTH(0, 0, -1),
    SOUTH(0, 0, 1),
    WEST(-1, 0, 0),
    EAST(1, 0, 0),
    ;

    public fun offset(pos: StructurePos): StructurePos = StructurePos(pos.x + dx, pos.y + dy, pos.z + dz)

    public val isHorizontal: Boolean get() = this == NORTH || this == SOUTH || this == WEST || this == EAST

    public fun opposite(): StructureDirection =
        when (this) {
            DOWN -> UP
            UP -> DOWN
            NORTH -> SOUTH
            SOUTH -> NORTH
            WEST -> EAST
            EAST -> WEST
        }

    /** // parity: Direction.mirror(Mirror). LEFT_RIGHT swaps NORTH/SOUTH (mirrors the Z axis); FRONT_BACK swaps EAST/WEST (mirrors the X axis). */
    public fun mirror(mirror: StructureMirror): StructureDirection =
        when (mirror) {
            StructureMirror.NONE -> this
            StructureMirror.LEFT_RIGHT ->
                when (this) {
                    NORTH -> SOUTH
                    SOUTH -> NORTH
                    else -> this
                }
            StructureMirror.FRONT_BACK ->
                when (this) {
                    EAST -> WEST
                    WEST -> EAST
                    else -> this
                }
        }

    /** // parity: Direction.getRotated / how a `Rotation` rotates a horizontal Direction around Y. UP/DOWN are unaffected. */
    public fun rotate(rotation: StructureRotation): StructureDirection {
        if (!isHorizontal) return this
        val cycle = listOf(NORTH, EAST, SOUTH, WEST)
        val steps =
            when (rotation) {
                StructureRotation.NONE -> 0
                StructureRotation.CLOCKWISE_90 -> 1
                StructureRotation.CLOCKWISE_180 -> 2
                StructureRotation.COUNTERCLOCKWISE_90 -> 3
            }
        val index = cycle.indexOf(this)
        return cycle[(index + steps) % 4]
    }

    public companion object {
        public fun fromName(name: String): StructureDirection? =
            entries.find { it.name.equals(name, ignoreCase = true) }
    }
}
