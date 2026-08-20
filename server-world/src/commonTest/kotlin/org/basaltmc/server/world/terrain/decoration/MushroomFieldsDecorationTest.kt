package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class MushroomFieldsDecorationTest {
    @Test
    fun officialIndexAndRandomizedOriginBiomeAreUsed() {
        assertEquals(79, MushroomFieldsDecoration.MUSHROOM_ISLAND_VEGETATION_INDEX)

        var acceptedOrigins: List<DecorationPos> = emptyList()
        for (seed in 0L..1_000L) {
            val origins = mutableListOf<DecorationPos>()
            MushroomFieldsDecoration.decorateFeature(
                flatChunk(),
                seed,
                environment { x -> if (x < 8) "mushroom_fields" else "the_void" },
            ) { origins += it }
            if (origins.isNotEmpty()) {
                acceptedOrigins = origins
                break
            }
        }

        assertTrue(acceptedOrigins.isNotEmpty())
        assertTrue(acceptedOrigins.all { it.x < 8 }, "unexpected origins: $acceptedOrigins")
    }

    @Test
    fun globalPlacementRejectsBiomeOutsideMushroomFields() {
        val origins = mutableListOf<DecorationPos>()
        MushroomFieldsDecoration.decorateFeature(flatChunk(), 1234L, environment { "plains" }) {
            origins += it
        }
        assertTrue(origins.isEmpty())
    }

    private fun flatChunk(): Chunk =
        Chunk(0, 0).also { chunk ->
            for (x in 0 until 16) {
                for (z in 0 until 16) chunk.setBlockStateId(x, 62, z, MYCELIUM_STATE_ID)
            }
        }

    private fun environment(biomeAtX: (Int) -> String): SurfaceRuleEnvironment =
        object : SurfaceRuleEnvironment {
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
            ): Float = 0.0f

            override fun estimatedSurfaceHeight(
                x: Int,
                z: Int,
            ): Int = WorldHeight.MAX_Y + 1

            override fun biomeAt(
                x: Int,
                y: Int,
                z: Int,
            ): String = "minecraft:${biomeAtX(x)}"

            override fun isSteep(
                x: Int,
                z: Int,
            ): Boolean = false
        }

    private companion object {
        val MYCELIUM_STATE_ID =
            Blocks.REGISTRY.byIdentifier(Identifier.vanilla("mycelium"))?.stateId
                ?: error("unknown block 'mycelium'")
    }
}
