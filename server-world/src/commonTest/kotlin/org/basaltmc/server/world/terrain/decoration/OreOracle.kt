package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.densityfunction.DensityCache
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the whole underground ores step. */
class OreOracle {
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
        ): Int = 100

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

    private fun stockedChunk(): Chunk {
        val chunk = Chunk(-4, 6)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in WorldHeight.MIN_Y until 100) {
                    // Deepslate low, stone above, so both target lists in a paired ore are reached.
                    chunk.setBlockStateId(x, y, z, if (y < 0) stateId("deepslate") else Blocks.STONE.stateId)
                }
                chunk.setBlockStateId(x, 100, z, Blocks.DIRT.stateId)
                chunk.setBlockStateId(x, 101, z, Blocks.GRASS_BLOCK.stateId)
            }
        }
        chunk.freezeWorldgenHeights()
        return chunk
    }

    private fun report(
        name: String,
        chunk: Chunk,
    ) {
        var checksum = -3750763034362895579L
        val counts = HashMap<String, Int>()
        for (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val stateId = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor stateId.toLong()) * 1099511628211L
                    val path = Blocks.byStateId(stateId)?.block?.path ?: continue
                    if (path.endsWith("_ore") || path == "clay" || path == "gravel" || path == "dirt") {
                        counts[path] = (counts[path] ?: 0) + 1
                    }
                }
            }
        }
        val deferred = chunk.takeGeneratedBlockWrites().size
        println(
            "ORACLE $name.checksum=$checksum deferred=$deferred counts=" +
                counts.entries.sortedBy { it.key }.joinToString(",") { "${it.key}:${it.value}" },
        )
    }

    @Test
    fun dump() {
        for (biome in listOf("minecraft:plains", "minecraft:dripstone_caves", "minecraft:badlands")) {
            val chunk = stockedChunk()
            OverworldOreDecoration.decorate(chunk, 77777L, FakeEnvironment(biome))
            report(biome.substringAfter(':'), chunk)
        }
        val infested = stockedChunk()
        OverworldOreDecoration.decorateInfested(infested, 77777L, FakeEnvironment("minecraft:grove"))
        report("infested", infested)

        // With a region behind it every blob keeps the part that falls outside the chunk, which is
        // the deferred pipeline and about a third of a large vein.
        val regional = stockedChunk()
        OverworldOreDecoration.decorate(
            regional,
            77777L,
            FakeEnvironment("minecraft:plains"),
            externalSurfaceHeight = { _, _ -> 100 },
            externalBlockState = { _, y, _ ->
                if (y < 0) {
                    stateId("deepslate")
                } else if (y < 100) {
                    Blocks.STONE.stateId
                } else {
                    Blocks.AIR.stateId
                }
            },
        )
        report("regional", regional)
    }
}
