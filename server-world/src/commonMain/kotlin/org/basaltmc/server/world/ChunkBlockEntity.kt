package org.basaltmc.server.world

import org.basaltmc.server.registry.BlockEntityTypes
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound

/**
 * One block entity of a chunk: where it sits, what kind it is, and the NBT it carries.
 *
 * [type] is the *block entity type*, not the block. Callers are free to hand in either, since a
 * block is what they have: a block that owns a differently named type is translated here rather
 * than at each of the twenty places that build one. Getting that wrong left a copper chest without
 * a protocol id, so it never reached the client and the block rendered as nothing.
 */
public class ChunkBlockEntity(
    public val localX: Int,
    public val y: Int,
    public val localZ: Int,
    type: Identifier,
    public val data: NbtCompound,
) {
    public val type: Identifier = BlockEntityTypes.ofBlock(type) ?: type

    /** The same block entity carrying different NBT, which is what a `/data` write leaves behind. */
    public fun withData(data: NbtCompound): ChunkBlockEntity = ChunkBlockEntity(localX, y, localZ, type, data)

    init {
        require(localX in 0..15) { "localX=$localX outside chunk" }
        require(localZ in 0..15) { "localZ=$localZ outside chunk" }
        require(y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) { "y=$y outside world" }
    }
}
