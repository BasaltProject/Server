package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.structure.StructurePos
import org.basaltmc.server.world.structure.StructureQueries
import org.basaltmc.server.world.structure.StructureSpawnOverrides
import org.basaltmc.wire.common.Identifier

/**
 * // parity: `NaturalSpawner.mobsAt` plus `ChunkGenerator.getMobsAt`. A start is only visible through
 * the position's own chunk, so an unloaded neighbour hides its structure.
 */
public class StructureSpawnLists(
    private val world: LoadedWorld,
) {
    private val queries =
        StructureQueries { chunkX, chunkZ ->
            world.state.chunkIfLoaded(chunkX, chunkZ)?.structures
        }

    /**
     * The override list for this position and category, or null to fall back to the biome.
     * An override with no spawns returns an empty list, which is not the same as null: it means
     * the structure silences the category.
     */
    public fun listAt(
        category: String,
        x: Int,
        y: Int,
        z: Int,
    ): List<SpawnerEntry>? {
        val pos = StructurePos(x, y, z)
        if (isInNetherFortressBounds(category, pos)) return FORTRESS_MONSTERS
        for (start in queries.startsIn(pos)) {
            val override = StructureSpawnOverrides.of(start.structure, category) ?: continue
            val inside =
                when (override.bounds) {
                    StructureSpawnOverrides.Bounds.PIECE -> start.hasPieceAt(pos)
                    StructureSpawnOverrides.Bounds.FULL -> start.contains(pos)
                }
            if (inside) return override.spawns.map(::toEntry)
        }
        return null
    }

    /**
     * // parity: `NaturalSpawner.isInNetherFortressBounds`, a second and separate fortress gate
     * that runs before the override table: standing on nether bricks anywhere in the fortress's
     * full bounds gives the fortress list, even outside every piece.
     */
    private fun isInNetherFortressBounds(
        category: String,
        pos: StructurePos,
    ): Boolean {
        if (category != "monster") return false
        val below = world.state.chunkIfLoaded(pos.x shr 4, pos.z shr 4) ?: return false
        val state = below.getBlockStateId(pos.x and 15, pos.y - 1, pos.z and 15)
        if (Blocks.byStateId(state)?.block?.path != "nether_bricks") return false
        return queries.startAt(pos, "fortress") != null
    }

    private fun toEntry(entry: StructureSpawnOverrides.SpawnEntry): SpawnerEntry =
        SpawnerEntry(Identifier.parse(entry.type), entry.weight, entry.minCount, entry.maxCount)

    private companion object {
        val FORTRESS_MONSTERS: List<SpawnerEntry> =
            StructureSpawnOverrides.of("fortress", "monster")!!.spawns.map {
                SpawnerEntry(Identifier.parse(it.type), it.weight, it.minCount, it.maxCount)
            }
    }
}
