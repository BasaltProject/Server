package org.basaltmc.world

import org.basaltmc.common.Identifier
import org.basaltmc.nbt.NbtCompound

/** Entity emitted by world generation and waiting for the runtime entity layer to materialize it. */
public data class ChunkGeneratedEntity(
    public val type: Identifier,
    public val x: Double,
    public val y: Double,
    public val z: Double,
    public val yaw: Float,
    public val pitch: Float,
    public val data: NbtCompound = NbtCompound(),
)
