package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class PaleMossDecorationTest {
    @Test
    fun globalPlacementFiltersTheRandomizedOriginBiome() {
        var acceptedOrigins: List<DecorationPos> = emptyList()
        for (seed in 0L..1_000L) {
            val origins = mutableListOf<DecorationPos>()
            PaleMossDecoration.decorateFeature(
                flatChunk(Blocks.GRASS_BLOCK.stateId),
                seed,
                environment { x -> if (x < 8) "pale_garden" else "the_void" },
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
    fun globalPlacementRejectsBiomeOutsidePaleGarden() {
        val origins = mutableListOf<DecorationPos>()
        PaleMossDecoration.decorateFeature(
            flatChunk(Blocks.GRASS_BLOCK.stateId),
            1234L,
            environment { "plains" },
        ) { origins += it }
        assertTrue(origins.isEmpty())
    }

    @Test
    fun configuredPatchAddsOneToTheOfficialTwoToFourRadiusProvider() {
        var reachedOuterRadius = false
        for (seed in 0L until 100L) {
            val chunk = flatChunk(Blocks.GRASS_BLOCK.stateId)
            PaleMossDecoration.placePatchAt(
                chunk,
                WorldgenRandom(XoroshiroRandom(seed)),
                DecorationPos(8, 63, 8),
            )
            for (x in 0 until 16) {
                for (z in 0 until 16) {
                    if (chunk.getBlockStateId(x, 62, z) != paleMossBlockStateId) continue
                    val distance = maxOf(kotlin.math.abs(x - 8), kotlin.math.abs(z - 8))
                    assertTrue(distance <= 5, "pale moss escaped effective radius at ($x,62,$z)")
                    if (distance == 5) reachedOuterRadius = true
                }
            }
        }
        assertTrue(reachedOuterRadius, "expected some sampled patch to reach effective radius five")
    }

    @Test
    fun replacesGroundAndDistributesVegetation() {
        val chunk = flatChunk(Blocks.GRASS_BLOCK.stateId)
        PaleMossDecoration.decorate(chunk, WorldgenRandom(XoroshiroRandom(9182L)))

        assertTrue(countAt(chunk, 62, paleMossBlockStateId) > 0)
        assertTrue(countPlants(chunk) > 0)
    }

    @Test
    fun doesNotReplaceBlocksOutsideMossReplaceableTag() {
        val sandStateId = stateId("sand")
        val chunk = flatChunk(sandStateId)
        PaleMossDecoration.decorate(chunk, WorldgenRandom(XoroshiroRandom(9182L)))

        assertEquals(256, countAt(chunk, 62, sandStateId))
        assertEquals(0, countAt(chunk, 62, paleMossBlockStateId))
    }

    @Test
    fun isDeterministicForSameSeed() {
        val first = flatChunk(Blocks.GRASS_BLOCK.stateId)
        val second = flatChunk(Blocks.GRASS_BLOCK.stateId)
        PaleMossDecoration.decorate(first, WorldgenRandom(XoroshiroRandom(77L)))
        PaleMossDecoration.decorate(second, WorldgenRandom(XoroshiroRandom(77L)))

        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 61..64) assertEquals(first.getBlockStateId(x, y, z), second.getBlockStateId(x, y, z))
            }
        }
    }

    private fun flatChunk(groundStateId: Int): Chunk {
        val chunk = Chunk(0, 0)
        for (x in 0 until 16) {
            for (z in 0 until 16) chunk.setBlockStateId(x, 62, z, groundStateId)
        }
        return chunk
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

    private fun countAt(
        chunk: Chunk,
        y: Int,
        stateId: Int,
    ): Int {
        var result = 0
        for (x in 0 until 16) {
            for (z in 0 until 16) if (chunk.getBlockStateId(x, y, z) == stateId) result++
        }
        return result
    }

    private fun countPlants(chunk: Chunk): Int =
        plantStateIds.sumOf { stateId -> countAt(chunk, 63, stateId) + countAt(chunk, 64, stateId) }

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    private val paleMossBlockStateId = stateId("pale_moss_block")
    private val plantStateIds = setOf(stateId("pale_moss_carpet"), stateId("short_grass"), stateId("tall_grass"))
}
