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

class CoralDecorationTest {
    @Test
    fun officialIndexAndRandomizedOriginBiomeAreUsed() {
        assertEquals(96, CoralDecoration.WARM_OCEAN_VEGETATION_INDEX)

        var acceptedOrigins: List<DecorationPos> = emptyList()
        val chunkX = coralChunkX()
        for (seed in 0L..1_000L) {
            val origins = mutableListOf<DecorationPos>()
            CoralDecoration.decorateFeature(
                oceanChunk(chunkX),
                seed,
                environment { x -> if ((x and 15) < 8) "warm_ocean" else "the_void" },
            ) { origins += it }
            if (origins.isNotEmpty()) {
                acceptedOrigins = origins
                break
            }
        }

        assertTrue(acceptedOrigins.isNotEmpty())
        assertTrue(acceptedOrigins.all { (it.x and 15) < 8 }, "unexpected origins: $acceptedOrigins")
    }

    @Test
    fun globalPlacementRejectsBiomeOutsideWarmOcean() {
        val origins = mutableListOf<DecorationPos>()
        CoralDecoration.decorateFeature(oceanChunk(coralChunkX()), 1234L, environment { "ocean" }) {
            origins += it
        }
        assertTrue(origins.isEmpty())
    }

    @Test
    fun coralCrossingChunkEdgeKeepsWorldCoordinatesAndExpectedWater() {
        var foundExternal = false
        for (seed in 0L..1_000L) {
            val chunk = oceanChunk(0)
            CoralDecoration.placeAt(
                chunk,
                WorldgenRandom(XoroshiroRandom(seed)),
                DecorationPos(15, 41, 8),
            )
            val external = chunk.takeGeneratedBlockWrites()
            if (external.isEmpty()) continue

            foundExternal = true
            assertTrue(external.any { it.x >= 16 })
            assertTrue(external.all { it.expectedStateId == Blocks.WATER.stateId })
            for (x in 0..5) {
                for (y in 41..50) {
                    for (z in 0 until 16) {
                        assertTrue(
                            chunk.getBlockStateId(x, y, z) !in CORAL_BLOCK_STATES,
                            "coral wrapped to local ($x,$y,$z)",
                        )
                    }
                }
            }
            break
        }
        assertTrue(foundExternal, "expected a sampled coral shape to cross the east chunk edge")
    }

    @Test
    fun coralAcceptsEveryStateOfTheWaterBlock() {
        val chunk = Chunk(0, 0)
        val origin = DecorationPos(8, 41, 8)
        chunk.setBlockStateId(origin.x, origin.y, origin.z, Blocks.WATER.stateId + 1)
        chunk.setBlockStateId(origin.x, origin.y + 1, origin.z, Blocks.WATER.stateId + 1)

        CoralDecoration.placeAt(
            chunk,
            WorldgenRandom(XoroshiroRandom(seedSelectingShape(1))),
            origin,
        )

        val placedPath = Blocks.byStateId(chunk.getBlockStateId(origin.x, origin.y, origin.z))?.block?.path
        assertTrue(placedPath?.endsWith("_coral_block") == true)
    }

    @Test
    fun coralDoesNotTreatWallFansAsMembersOfTheCoralsTag() {
        val chunk = Chunk(0, 0)
        val origin = DecorationPos(8, 41, 8)
        val wallFan = stateId("tube_coral_wall_fan")
        chunk.setBlockStateId(origin.x, origin.y, origin.z, wallFan)
        chunk.setBlockStateId(origin.x, origin.y + 1, origin.z, Blocks.WATER.stateId)

        CoralDecoration.placeAt(
            chunk,
            WorldgenRandom(XoroshiroRandom(seedSelectingShape(1))),
            origin,
        )

        assertEquals(wallFan, chunk.getBlockStateId(origin.x, origin.y, origin.z))
    }

    private fun oceanChunk(chunkX: Int): Chunk =
        Chunk(chunkX, 0).also { chunk ->
            for (x in 0 until 16) {
                for (z in 0 until 16) {
                    for (y in WorldHeight.MIN_Y..40) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                    for (y in 41 until 63) chunk.setBlockStateId(x, y, z, Blocks.WATER.stateId)
                }
            }
        }

    private fun coralChunkX(): Int =
        (-20..20).first { candidate ->
            NoiseBasedCountPlacement(20, 400.0)
                .getPositions(
                    PlacementContext(Chunk(candidate, 0)),
                    WorldgenRandom(XoroshiroRandom(0L)),
                    DecorationPos(candidate * 16, 0, 0),
                ).any()
        }

    private fun seedSelectingShape(index: Int): Long =
        (0L..1_000L).first { seed -> WorldgenRandom(XoroshiroRandom(seed)).nextInt(3) == index }

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

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
        val CORAL_BLOCK_STATES =
            listOf("tube", "brain", "bubble", "fire", "horn")
                .mapNotNull { name ->
                    Blocks.REGISTRY.byIdentifier(
                        org.basaltmc.wire.common.Identifier
                            .vanilla("${name}_coral_block"),
                    )
                }.map { it.stateId }
                .toSet()
    }
}
