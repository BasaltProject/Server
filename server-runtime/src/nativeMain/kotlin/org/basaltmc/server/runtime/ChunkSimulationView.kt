package org.basaltmc.server.runtime

import org.basaltmc.server.runtime.player.PlayerRegistry
import org.basaltmc.server.runtime.sched.Region
import org.basaltmc.server.runtime.world.LoadedWorld

/** parity: `simulation-distance` in `server.properties`, ten chunks. */
private const val VANILLA_SIMULATION_DISTANCE = 10

/** The chunks each tick method walks that are meant to be simulated, not merely loaded. */
public class ChunkSimulationView(
    private val playerRegistry: PlayerRegistry,
) {
    /**
     * The chunks around whoever is in the world, without repeats.
     *
     * parity: vanilla's simulation distance, which is ten by default and separate from the view
     * distance. This build has no setting of its own for it yet, so it follows the view distance up
     * to vanilla's own default and no further.
     */
    public fun simulatedChunks(
        world: LoadedWorld,
        region: Region? = null,
    ): List<IntArray> {
        val radius = minOf(world.settings.viewDistance, VANILLA_SIMULATION_DISTANCE)
        val seen = HashSet<Long>()
        val chunks = mutableListOf<IntArray>()
        for (player in playerRegistry.snapshot()) {
            if (player.worldName != world.name) continue
            // Only the players this region owns: the chunks around a player are always in the same
            // region as the player (a region is made of the sections that are loaded and touching,
            // and a player loads the ones around them), so walking every player's square in every
            // region was the same answer worked out a hundred times.
            if (region != null && !region.ownsPosition(player.x, player.z)) continue
            val centreX = player.x.toInt() shr 4
            val centreZ = player.z.toInt() shr 4
            for (x in centreX - radius..centreX + radius) {
                for (z in centreZ - radius..centreZ + radius) {
                    // A chunk is simulated by whoever owns it, so two regions never tick the same one.
                    if (region != null && !region.owns(x, z)) continue
                    if (seen.add((x.toLong() shl 32) or (z.toLong() and 0xFFFFFFFFL))) chunks += intArrayOf(x, z)
                }
            }
        }
        return chunks
    }
}
