package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.runtime.entity.ServerEntity
import org.basaltmc.wire.common.Identifier

/**
 * The rail that notices a minecart standing on it.
 *
 * parity: `DetectorRailBlock.checkPressed`: while something is on it, it is on and books twenty
 * ticks to look again; the tick it finds nothing, it goes out. It feeds every side weakly and the
 * block underneath strongly, which is what `Redstone` already reads off its own state.
 */
public object DetectorRails {
    /** parity: the twenty ticks a pressed rail waits before checking again. */
    public const val RECHECK_TICKS: Int = 20

    private val RAIL = Identifier.vanilla("detector_rail")

    /** parity: `AbstractMinecart`, of which any kind presses the rail. */
    private val MINECARTS =
        setOf(
            "minecart",
            "chest_minecart",
            "furnace_minecart",
            "tnt_minecart",
            "hopper_minecart",
            "spawner_minecart",
            "command_block_minecart",
        )

    public fun isMinecart(type: Identifier): Boolean = type.path in MINECARTS

    /**
     * Looks at what is standing on the rail and answers the change, if it changed.
     *
     * parity: `checkPressed` is called both by a cart arriving and by the rail's own scheduled tick,
     * which is why this takes the whole answer rather than a direction.
     */
    public fun check(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        entities: List<ServerEntity>,
    ): BlockChange? {
        val stateId = world.state.getBlockState(x, y, z) ?: return null
        val block = BlockStateDefinitions.blockOf(stateId) ?: return null
        if (block != RAIL) return null
        val powered = BlockStateDefinitions.propertiesOf(stateId)["powered"] == "true"
        val occupied =
            entities.any {
                !it.removed &&
                    isMinecart(it.type) &&
                    it.x >= x &&
                    it.x <= x + 1 &&
                    it.z >= z &&
                    it.z <= z + 1 &&
                    it.y >= y - 1 &&
                    it.y <= y + 1
            }
        if (occupied) world.state.scheduleBlockTick(x, y, z, RAIL, RECHECK_TICKS)
        if (occupied == powered) return null
        val turned =
            BlockStateDefinitions.stateIdOf(block, mapOf("powered" to occupied.toString()), stateId) ?: return null
        if (!world.state.setBlockState(x, y, z, turned)) return null
        return BlockChange(x, y, z, turned)
    }
}
