package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.densityfunction.DensityCache
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the taiga ground cover and the mushroom fields. */
class TaigaMushroomOracle {
    private inner class FakeEnvironment(
        private val biome: String,
    ) : SurfaceRuleEnvironment {
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
        ): Float = 0.5f

        override fun positionalRandom(
            x: Int,
            y: Int,
            z: Int,
        ): BasaltRandom = XoroshiroRandom(x * 31L + z)

        override fun estimatedSurfaceHeight(
            x: Int,
            z: Int,
        ): Int = 70

        override fun biomeAt(
            x: Int,
            y: Int,
            z: Int,
        ): String = biome

        override fun noiseBiomeAt(
            quartX: Int,
            quartY: Int,
            quartZ: Int,
            cache: DensityCache?,
        ): String = biome

        override fun isSteep(
            x: Int,
            z: Int,
        ): Boolean = false

        override fun clayBand(
            x: Int,
            y: Int,
            z: Int,
        ): Int = 0
    }

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    /** Grass over dirt with a rolling surface and a mycelium band, so both ground rules apply. */
    private fun groundChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                val worldX = chunkX * 16 + x
                val worldZ = chunkZ * 16 + z
                val surface = 66 + (worldX * 3 + worldZ * 5).mod(4)
                for (y in 60 until surface) chunk.setBlockStateId(x, y, z, stateId("dirt"))
                val top = if ((worldX + worldZ).mod(9) == 0) stateId("mycelium") else Blocks.GRASS_BLOCK.stateId
                chunk.setBlockStateId(x, surface, z, top)
            }
        }
        return chunk
    }

    private val ignored = setOf("dirt", "grass_block", "mycelium", "air")

    private fun fold(chunk: Chunk): Pair<Long, Int> {
        var checksum = -3750763034362895579L
        var placed = 0
        for (y in 60..95) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    val path = Blocks.byStateId(id)?.block?.path
                    if (path != null && path !in ignored) placed++
                }
            }
        }
        return checksum to placed
    }

    private fun report(
        name: String,
        count: Int = 6,
        step: (Chunk, WorldgenRandom) -> Unit,
    ) {
        var checksum = -3750763034362895579L
        var placed = 0
        var deferred = 0
        var trailing = 0
        for (chunkIndex in 0 until count) {
            val chunk = groundChunk(chunkIndex * 2 - 3, chunkIndex * -3 + 8)
            val random = WorldgenRandom(XoroshiroRandom(9127L + chunkIndex))
            step(chunk, random)
            val (chunkChecksum, chunkPlaced) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            placed += chunkPlaced
            deferred += chunk.takeGeneratedBlockWrites().size
            trailing = random.nextInt()
        }
        println("ORACLE $name placed=$placed deferred=$deferred checksum=$checksum after=$trailing")
    }

    @Test
    fun dump() {
        report("largeFern", 24) { chunk, random -> TaigaGroundDecoration.LARGE_FERN.decorate(chunk, random) }
        report("normalGrass") { chunk, random -> TaigaGroundDecoration.NORMAL.decorate(chunk, random) }
        report("oldGrowth") { chunk, random -> TaigaGroundDecoration.OLD_GROWTH.decorate(chunk, random) }
        report("deadBush") { chunk, random -> TaigaGroundDecoration.OLD_GROWTH_DEAD_BUSH.decorate(chunk, random) }
        report("commonBerry", 128) { chunk, random -> TaigaGroundDecoration.COMMON_BERRY.decorate(chunk, random) }
        report("rareBerry", 512) { chunk, random -> TaigaGroundDecoration.RARE_BERRY.decorate(chunk, random) }
        report("mushroomHuge") { chunk, random -> MushroomFieldsDecoration.HUGE_ONLY.decorate(chunk, random) }
        report("mushroomVegetal") { chunk, random -> MushroomFieldsDecoration.decorate(chunk, random) }

        for (biome in listOf("minecraft:mushroom_fields", "minecraft:plains")) {
            var checksum = -3750763034362895579L
            var placed = 0
            for (chunkIndex in 0 until 6) {
                val chunk = groundChunk(chunkIndex * 2 - 3, chunkIndex * -3 + 8)
                MushroomFieldsDecoration.decorate(chunk, 5431L + chunkIndex, FakeEnvironment(biome))
                val (chunkChecksum, chunkPlaced) = fold(chunk)
                checksum = (checksum xor chunkChecksum) * 1099511628211L
                placed += chunkPlaced
            }
            val label = if (biome.endsWith("plains")) "mushroomPlains" else "mushroomStep"
            println("ORACLE $label placed=$placed deferred=0 checksum=$checksum after=0")
        }
    }
}
