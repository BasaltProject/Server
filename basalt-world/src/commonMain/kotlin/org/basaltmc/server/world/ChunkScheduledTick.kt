package org.basaltmc.server.world

import org.basaltmc.wire.common.Identifier

/** // parity: SavedTick, owned by one chunk; X/Z are local until disk serialization. */
public data class ChunkScheduledTick(
    public val type: Identifier,
    public val localX: Int,
    public val y: Int,
    public val localZ: Int,
    public val delay: Int,
    public val priority: Int = 0,
) {
    init {
        require(localX in 0..15) { "localX=$localX outside chunk" }
        require(localZ in 0..15) { "localZ=$localZ outside chunk" }
        require(y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) { "y=$y outside world" }
        require(priority in -3..3) { "priority=$priority outside vanilla TickPriority" }
    }
}
