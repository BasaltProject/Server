package org.basaltmc.world

import org.basaltmc.common.Identifier
import org.basaltmc.nbt.NbtCompound

/** A block entity owned by one [Chunk], with chunk-local X/Z and custom update/save NBT. */
public data class ChunkBlockEntity(
    public val localX: Int,
    public val y: Int,
    public val localZ: Int,
    public val type: Identifier,
    public val data: NbtCompound,
) {
    init {
        require(localX in 0..15) { "localX=$localX outside chunk" }
        require(localZ in 0..15) { "localZ=$localZ outside chunk" }
        require(y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) { "y=$y outside world" }
    }
}
