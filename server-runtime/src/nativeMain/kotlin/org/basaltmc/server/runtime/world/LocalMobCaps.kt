package org.basaltmc.server.runtime.world

import org.basaltmc.server.runtime.entity.MobCategory
import kotlin.math.floor

/** A non-spectator player that contributes a vanilla local natural-spawn cap. */
public class MobCapPlayer(
    public val id: Int,
    public val x: Double,
    public val z: Double,
)

/**
 * Per-player natural mob caps around spawning chunks.
 *
 * parity: `LocalMobCapCalculator`: every existing/spawned mob counts for every player whose
 * distance to that chunk's centre is below 128 blocks, and a chunk may spawn when any nearby
 * player's count is below the category's unscaled per-player maximum.
 */
public class LocalMobCaps(
    players: List<MobCapPlayer>,
) {
    private val players = players.toList()
    private val counts = players.associate { it.id to IntArray(MobCategory.entries.size) }
    private val nearbyCache = mutableMapOf<Long, List<MobCapPlayer>>()

    public fun canSpawn(
        category: MobCategory,
        chunkX: Int,
        chunkZ: Int,
    ): Boolean = nearby(chunkX, chunkZ).any { counts.getValue(it.id)[category.ordinal] < category.maxPerChunk }

    /** parity: DistanceManager's radius-eight square used for the global spawnable-chunk count. */
    public fun isSpawnCandidate(
        chunkX: Int,
        chunkZ: Int,
    ): Boolean =
        players.any {
            val playerChunkX = floor(it.x).toInt() shr 4
            val playerChunkZ = floor(it.z).toInt() shr 4
            kotlin.math.abs(chunkX - playerChunkX) <= GLOBAL_CAP_RADIUS &&
                kotlin.math.abs(chunkZ - playerChunkZ) <= GLOBAL_CAP_RADIUS
        }

    public fun add(
        category: MobCategory,
        chunkX: Int,
        chunkZ: Int,
    ) {
        if (category == MobCategory.MISC) return
        for (player in nearby(chunkX, chunkZ)) counts.getValue(player.id)[category.ordinal]++
    }

    internal fun count(
        playerId: Int,
        category: MobCategory,
    ): Int = counts.getValue(playerId)[category.ordinal]

    private fun nearby(
        chunkX: Int,
        chunkZ: Int,
    ): List<MobCapPlayer> =
        nearbyCache.getOrPut(ChunkGenerationQueue.pack(chunkX, chunkZ)) {
            val centreX = (chunkX shl 4) + 8.0
            val centreZ = (chunkZ shl 4) + 8.0
            players.filter {
                val dx = centreX - it.x
                val dz = centreZ - it.z
                dx * dx + dz * dz < SPAWNING_DISTANCE_SQUARED
            }
        }

    private companion object {
        const val GLOBAL_CAP_RADIUS = 8
        const val SPAWNING_DISTANCE_SQUARED = 128.0 * 128.0
    }
}
