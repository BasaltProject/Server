package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.wire.common.Identifier

/**
 * The white top a block of grass wears while there is snow on it.
 *
 * parity: `SnowyBlock`, whose `snowy` property is read off the block above, both when it is placed
 * and every time that neighbour changes. Without the second half, breaking a snow layer leaves the
 * grass white for ever, which is what a real client showed.
 */
public object SnowyBlocks {
    /** parity: `BlockTags.SNOW`, which is what `isSnowySetting` asks. */
    private val SNOW =
        setOf(
            Identifier.vanilla("snow"),
            Identifier.vanilla("snow_block"),
            Identifier.vanilla("powder_snow"),
        )

    public fun isSnow(stateId: Int): Boolean = BlockStateDefinitions.blockOf(stateId) in SNOW

    /** Whether this block wears snow at all: grass, podzol and mycelium do. */
    public fun wearsSnow(stateId: Int): Boolean =
        BlockStateDefinitions
            .of(BlockStateDefinitions.blockOf(stateId) ?: return false)
            ?.valuesOf("snowy")
            ?.isNotEmpty() == true

    /**
     * The block at [x], [y], [z] with its `snowy` set from whatever is above it now, or null when
     * it does not wear snow or already looks right.
     */
    public fun updated(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Int? {
        val state = world.state.getBlockState(x, y, z) ?: return null
        if (!wearsSnow(state)) return null
        val above = world.state.getBlockState(x, y + 1, z) ?: return null
        val block = BlockStateDefinitions.blockOf(state) ?: return null
        val wanted =
            BlockStateDefinitions.stateIdOf(block, mapOf("snowy" to isSnow(above).toString()), fromStateId = state)
        return wanted?.takeIf { it != state }
    }
}
