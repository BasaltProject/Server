package org.basaltmc.server.world.terrain

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.densityfunction.DensityCache
import org.basaltmc.server.world.terrain.decoration.OverworldLakeDecoration
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/**
 * Step 1 was wired long after its bodies were, so what this pins down is the wiring: the four LAKES
 * nodes run at their own global indices. A wrong index would still generate lakes, from a different
 * seed, so the counts are exact rather than "more than zero".
 */
class LakeStepTest {
    private object FlatPlains : SurfaceRuleEnvironment {
        override val biomeZoomSeed: Long? = null

        override fun surfaceDepthNoise(
            x: Int,
            z: Int,
        ): Int = 0

        override fun surfaceSecondaryNoise(
            x: Int,
            z: Int,
        ): Double = 0.0

        override fun sample2d(
            noiseId: String,
            x: Int,
            z: Int,
        ): Double = 0.0

        override fun sample3d(
            noiseId: String,
            x: Int,
            y: Int,
            z: Int,
        ): Double = 0.0

        override fun verticalGradientRandom(
            name: String,
            x: Int,
            y: Int,
            z: Int,
        ): Float = 0f

        override fun estimatedSurfaceHeight(
            x: Int,
            z: Int,
        ): Int = SURFACE_Y

        override fun biomeAt(
            x: Int,
            y: Int,
            z: Int,
        ): String = "minecraft:plains"

        override fun noiseBiomeAt(
            quartX: Int,
            quartY: Int,
            quartZ: Int,
            cache: DensityCache?,
        ): String = "minecraft:plains"

        override fun isSteep(
            x: Int,
            z: Int,
        ): Boolean = false

        override fun clayBand(
            x: Int,
            y: Int,
            z: Int,
        ): Int = Blocks.AIR.stateId
    }

    private val stone =
        Blocks.REGISTRY
            .byIdentifier(
                org.basaltmc.wire.common.Identifier
                    .vanilla("stone"),
            )!!
            .stateId

    /** Stone up to the surface and air above it, which is all the LAKES chain looks at. */
    private fun groundChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk =
        Chunk(chunkX, chunkZ).apply {
            for (x in 0 until 16) {
                for (z in 0 until 16) {
                    for (y in WorldHeight.MIN_Y..SURFACE_Y) setBlockStateId(x, y, z, stone)
                }
            }
        }

    /** The real regional view, over the same flat ground, which is what generation installs. */
    private fun neighbours() = NeighbourTerrainCache(generateTerrain = ::groundChunk)

    private fun decorate(
        chunk: Chunk,
        cache: NeighbourTerrainCache,
    ) {
        val view = cache.around(chunk.chunkX, chunk.chunkZ)
        chunk.withExternalDecorationBlockState(reader = { x, y, z -> view.blockStateIdAt(x, y, z) }) {
            OverworldLakeDecoration.decorate(chunk, SEED, FlatPlains)
        }
    }

    private fun lavaChunksOver(span: Int): Int {
        val cache = neighbours()
        var withLava = 0
        for (chunkX in 0 until span) {
            for (chunkZ in 0 until span) {
                val chunk = groundChunk(chunkX, chunkZ)
                decorate(chunk, cache)
                if (holdsLava(chunk)) withLava++
            }
        }
        return withLava
    }

    private fun holdsLava(chunk: Chunk): Boolean {
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in WorldHeight.MIN_Y..SURFACE_Y) {
                    if (chunk.getBlockStateId(x, y, z) == Blocks.LAVA.stateId) return true
                }
            }
        }
        return false
    }

    @Test
    fun `step 1 puts lava lakes in the ground`() {
        val withLava = lavaChunksOver(SPAN)
        assertTrue(withLava > 0, "the LAKES step must carve lava somewhere in ${SPAN * SPAN} chunks")
        assertEquals(EXPECTED_CHUNKS_WITH_LAVA, withLava, "the feature seeding of step 1 changed")
    }

    @Test
    fun `the same chunk decorates the same way twice`() {
        val once = groundChunk(3, 5).also { decorate(it, neighbours()) }
        val twice = groundChunk(3, 5).also { decorate(it, neighbours()) }
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in WorldHeight.MIN_Y..SURFACE_Y) {
                    assertEquals(
                        once.getBlockStateId(x, y, z),
                        twice.getBlockStateId(x, y, z),
                        "decoration is not deterministic at $x/$y/$z",
                    )
                }
            }
        }
    }

    private companion object {
        const val SEED = 987654321L
        const val SURFACE_Y = 70
        const val SPAN = 16
        const val EXPECTED_CHUNKS_WITH_LAVA = 12
    }
}
