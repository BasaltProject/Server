package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment

/**
 * The Overworld UNDERGROUND_STRUCTURES nodes, step 3.
 *
 * Four features share the step, and their order is forced by a single chain across the biomes that
 * declare them: `fossil_upper`, `fossil_lower`, `monster_room`, `monster_room_deep`. Only the two
 * rooms are here; the fossils have no body yet, but they keep their indices so adding them later
 * does not move the rooms' seeds.
 */
public object OverworldUndergroundStructures {
    public fun decorate(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
    ) {
        for (spec in SPECS) decorateFeature(chunk, seed, environment, spec)
    }

    private fun decorateFeature(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
        spec: RoomSpec,
    ) {
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        random.setFeatureSeed(decorationSeed, spec.index, UNDERGROUND_STRUCTURES_STEP)
        repeat(spec.count) {
            val x = chunk.chunkX * 16 + random.nextInt(16)
            val z = chunk.chunkZ * 16 + random.nextInt(16)
            val y = spec.minY + random.nextInt(spec.maxY - spec.minY + 1)
            if (environment.biomeAt(x, y, z).substringAfter(':') !in ALL_BIOMES) return@repeat
            StandaloneFeatures.placeAt(chunk, random, DecorationPos(x, y, z), "monster_room")
        }
    }

    private class RoomSpec(
        val index: Int,
        val count: Int,
        val minY: Int,
        val maxY: Int,
    )

    private val SPECS =
        listOf(
            // `below_top 0`, the highest block the world can generate.
            RoomSpec(index = 2, count = 10, minY = 0, maxY = BELOW_TOP),
            // `above_bottom 6` up to y -1, the deep pass.
            RoomSpec(index = 3, count = 4, minY = WorldHeight.MIN_Y + 6, maxY = -1),
        )

    private const val UNDERGROUND_STRUCTURES_STEP = 3
    private const val BELOW_TOP = 319
}
