package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.Blocks
import org.basaltmc.wire.common.Identifier

/**
 * The water a sponge drinks the moment it is put down.
 *
 * parity: `SpongeBlock.onPlace` and its `removeWaterBreadthFirstSearch`: six blocks out, sixty-five
 * of them at most, counted in the order a breadth-first walk reaches them. Water that a block is
 * merely logged with is drained out of it, and the kelp and seagrass standing in the way come out
 * with it. A sponge that drank anything at all turns wet.
 */
public object Sponges {
    /** parity: `SpongeBlock`'s own two limits. */
    private const val REACH = 6
    private const val MAX_ABSORBED = 65

    /** parity: the plants the search pulls up rather than walking past. */
    private val UPROOTED = setOf("kelp", "kelp_plant", "seagrass", "tall_seagrass")

    private val FACES =
        listOf(
            intArrayOf(0, -1, 0),
            intArrayOf(0, 1, 0),
            intArrayOf(0, 0, -1),
            intArrayOf(0, 0, 1),
            intArrayOf(-1, 0, 0),
            intArrayOf(1, 0, 0),
        )

    /**
     * parity: `WetSpongeBlock.onPlace` over the `water_evaporates` attribute, which is the Nether's:
     * a wet sponge put down there dries out on the spot.
     */
    public fun dryIfEvaporating(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        onChanged: (stateId: Int, x: Int, y: Int, z: Int) -> Unit,
    ) {
        if (!world.declaration.dimension.hasCeiling) return
        if (BlockStateDefinitions.blockOf(world.state.getBlockState(x, y, z) ?: 0)?.path != "wet_sponge") return
        val dry = BlockStateDefinitions.stateIdOf(Identifier.vanilla("sponge"), emptyMap()) ?: return
        if (!world.state.setBlockState(x, y, z, dry)) return
        onChanged(dry, x, y, z)
    }

    /**
     * Drinks whatever is around ([x], [y], [z]) and turns the sponge wet if it drank anything.
     * Reports every block that changed, the sponge included, so the caller can tell the clients.
     */
    public fun absorb(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        onChanged: (stateId: Int, x: Int, y: Int, z: Int) -> Unit,
    ) {
        if (BlockStateDefinitions.blockOf(world.state.getBlockState(x, y, z) ?: 0)?.path != "sponge") return
        val visited = HashSet<Long>()
        var frontier = mutableListOf(intArrayOf(x, y, z))
        visited += packed(x, y, z)
        var absorbed = 0
        for (step in 1..REACH) {
            val next = mutableListOf<IntArray>()
            for (position in frontier) {
                for (face in FACES) {
                    if (absorbed >= MAX_ABSORBED) break
                    val nx = position[0] + face[0]
                    val ny = position[1] + face[1]
                    val nz = position[2] + face[2]
                    if (!visited.add(packed(nx, ny, nz))) continue
                    if (!drink(world, nx, ny, nz, onChanged)) continue
                    absorbed++
                    next += intArrayOf(nx, ny, nz)
                }
            }
            if (next.isEmpty() || absorbed >= MAX_ABSORBED) break
            frontier = next
        }
        if (absorbed == 0) return
        val wet = BlockStateDefinitions.stateIdOf(Identifier.vanilla("wet_sponge"), emptyMap()) ?: return
        if (!world.state.setBlockState(x, y, z, wet)) return
        onChanged(wet, x, y, z)
    }

    /** Takes the water out of one position, answering whether there was any to take. */
    private fun drink(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        onChanged: (stateId: Int, x: Int, y: Int, z: Int) -> Unit,
    ): Boolean {
        val stateId = world.state.getBlockState(x, y, z) ?: return false
        val path = BlockStateDefinitions.blockOf(stateId)?.path ?: return false
        if (path == "water") {
            if (!world.state.setBlockState(x, y, z, Blocks.AIR.stateId)) return false
            onChanged(Blocks.AIR.stateId, x, y, z)
            return true
        }
        if (path in UPROOTED) {
            if (!world.state.setBlockState(x, y, z, Blocks.AIR.stateId)) return false
            onChanged(Blocks.AIR.stateId, x, y, z)
            return true
        }
        // parity: `BucketPickup.pickupBlock`, which drains a logged block instead of breaking it.
        if (BlockStateDefinitions.propertiesOf(stateId)["waterlogged"] != "true") return false
        val block = BlockStateDefinitions.blockOf(stateId) ?: return false
        val drained = BlockStateDefinitions.stateIdOf(block, mapOf("waterlogged" to "false"), stateId) ?: return false
        if (!world.state.setBlockState(x, y, z, drained)) return false
        onChanged(drained, x, y, z)
        return true
    }

    private fun packed(
        x: Int,
        y: Int,
        z: Int,
    ): Long = (x.toLong() and 0x3FFFFFF shl 38) or (z.toLong() and 0x3FFFFFF shl 12) or (y.toLong() and 0xFFF)
}
