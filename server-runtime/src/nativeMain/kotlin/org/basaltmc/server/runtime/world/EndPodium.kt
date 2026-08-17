package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier
import kotlin.math.sqrt

/**
 * The bedrock podium at the middle of the End, where the exit portal stands.
 *
 * parity: `EndPodiumFeature`, placed the way `EnderDragonFight` places it when the End is first
 * loaded: closed, because in vanilla the dragon is still alive at that point. There is no dragon
 * here to open it, which is why an operator can ask for it open (see `worlds.toml`); that is a
 * deliberate divergence and off by default.
 */
public object EndPodium {
    public const val PODIUM_RADIUS: Int = 4
    private const val RIM_RADIUS = 2.5
    private const val OUTER_RADIUS = 3.5
    private const val PILLAR_HEIGHT = 4
    private const val CEILING = 32
    private const val LOWEST_ORIGIN_Y = 63

    private val WALL_TORCHES: Map<String, Int> =
        listOf("north", "south", "west", "east").associateWith { facing ->
            BlockStateDefinitions.stateIdOf(Identifier.vanilla("wall_torch"), mapOf("facing" to facing))
                ?: Blocks.AIR.stateId
        }

    /** Whether the podium is already there, which is what stops it being rebuilt on every load. */
    public fun exists(world: LoadedWorld): Boolean {
        for (y in world.minY..world.maxY) {
            if (world.state.getBlockState(0, y, 0) == Blocks.BEDROCK.stateId) return true
        }
        return false
    }

    /** Builds it if it is not there yet, and answers where its top block ended up. */
    public fun ensure(
        world: LoadedWorld,
        active: Boolean,
    ): IntArray? {
        for (chunkX in -1..1) {
            for (chunkZ in -1..1) world.state.getOrGenerateChunk(chunkX, chunkZ)
        }
        if (exists(world)) return null
        return place(world, originY(world), active)
    }

    /**
     * parity: `EnderDragonFight.spawnExitPortal`: the surface at the origin, one block down, then
     * down again through any bedrock, and never below the bottom of the world.
     */
    private fun originY(world: LoadedWorld): Int {
        val chunk = world.state.getOrGenerateChunk(0, 0)
        var y = chunk.heightAt(0, 0, Chunk.HeightmapType.MOTION_BLOCKING_NO_LEAVES) - 1
        while (world.state.getBlockState(0, y, 0) == Blocks.BEDROCK.stateId && y > LOWEST_ORIGIN_Y) y--
        return maxOf(world.minY + 1, y)
    }

    private fun place(
        world: LoadedWorld,
        originY: Int,
        active: Boolean,
    ): IntArray {
        val portal = checkNotNull(Blocks.REGISTRY.idOf(Identifier.vanilla("end_portal")))
        for (x in -PODIUM_RADIUS..PODIUM_RADIUS) {
            for (z in -PODIUM_RADIUS..PODIUM_RADIUS) {
                val distance = sqrt((x * x + z * z).toDouble())
                val insideRim = distance <= RIM_RADIUS
                if (!insideRim && distance > OUTER_RADIUS) continue
                for (y in originY - 1..originY + CEILING) {
                    val stateId =
                        when {
                            y < originY -> if (insideRim) Blocks.BEDROCK.stateId else Blocks.END_STONE.stateId
                            y > originY -> Blocks.AIR.stateId
                            !insideRim -> Blocks.BEDROCK.stateId
                            active -> portal
                            else -> Blocks.AIR.stateId
                        }
                    world.state.setBlockState(x, y, z, stateId)
                }
            }
        }
        for (height in 0 until PILLAR_HEIGHT) {
            world.state.setBlockState(0, originY + height, 0, Blocks.BEDROCK.stateId)
        }
        // parity: the four torches on the pillar, each facing out of the side it hangs on.
        val torchY = originY + 2
        world.state.setBlockState(0, torchY, -1, WALL_TORCHES.getValue("north"))
        world.state.setBlockState(0, torchY, 1, WALL_TORCHES.getValue("south"))
        world.state.setBlockState(-1, torchY, 0, WALL_TORCHES.getValue("west"))
        world.state.setBlockState(1, torchY, 0, WALL_TORCHES.getValue("east"))
        return intArrayOf(0, originY, 0)
    }
}
