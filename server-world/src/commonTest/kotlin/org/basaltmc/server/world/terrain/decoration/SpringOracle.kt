package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.densityfunction.DensityCache
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the three fluid springs. */
class SpringOracle {
    private inner class FakeEnvironment(private val biome: String) : SurfaceRuleEnvironment {
        override fun surfaceDepthNoise(x: Int, z: Int): Int = 0
        override fun surfaceSecondaryNoise(x: Int, z: Int): Double = 0.0
        override fun sample2d(noiseId: String, x: Int, z: Int): Double = 0.0
        override fun sample3d(noiseId: String, x: Int, y: Int, z: Int): Double = 0.0
        override fun verticalGradientRandom(name: String, x: Int, y: Int, z: Int): Float = 0.5f
        override fun positionalRandom(x: Int, y: Int, z: Int): BasaltRandom = XoroshiroRandom(x * 31L + z)
        override fun estimatedSurfaceHeight(x: Int, z: Int): Int = 70
        override fun biomeAt(x: Int, y: Int, z: Int): String = biome
        override fun noiseBiomeAt(quartX: Int, quartY: Int, quartZ: Int, cache: DensityCache?): String = biome
        override fun isSteep(x: Int, z: Int): Boolean = false
        override fun clayBand(x: Int, y: Int, z: Int): Int = 0
    }

    /**
     * Solid stone with single-block pockets punched through it: a spring needs exactly four solid
     * sides and exactly one hole, so neither a solid mass nor an open cave produces any.
     */
    private fun pocketedChunk(chunkX: Int, chunkZ: Int, frozen: Boolean): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        val fill = if (frozen) stateId("packed_ice") else Blocks.STONE.stateId
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in -60 until 120) chunk.setBlockStateId(x, y, z, fill)
            }
        }
        // One hole beside every third column, which is the shape a spring accepts.
        for (x in 1 until 15 step 3) {
            for (z in 1 until 15 step 3) {
                for (y in -50 until 110 step 7) {
                    chunk.setBlockStateId(x + 1, y, z, Blocks.AIR.stateId)
                }
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Triple<Long, Int, Int> {
        var checksum = -3750763034362895579L
        var water = 0
        var lava = 0
        for (y in -60..120) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    if (id == Blocks.WATER.stateId) water++
                    if (id == Blocks.LAVA.stateId) lava++
                }
            }
        }
        return Triple(checksum, water, lava)
    }

    private fun report(name: String, biome: String, frozen: Boolean, body: (Chunk, FakeEnvironment) -> Unit) {
        var checksum = -3750763034362895579L
        var water = 0
        var lava = 0
        var ticks = 0
        for (index in 0 until 30) {
            val chunk = pocketedChunk(index * 3 - 15, index * -5 + 10, frozen)
            body(chunk, FakeEnvironment(biome))
            val (chunkChecksum, chunkWater, chunkLava) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            water += chunkWater
            lava += chunkLava
            ticks += chunk.scheduledFluidTicks().size
        }
        println("ORACLE $name water=$water lava=$lava ticks=$ticks checksum=$checksum")
    }

    @Test
    fun dump() {
        report("plains", "minecraft:plains", false) { c, e -> OverworldSpringDecoration.decorate(c, 30303L, e) }
        report("grove", "minecraft:grove", true) { c, e -> OverworldSpringDecoration.decorate(c, 30303L, e) }
        // the_void is in neither spring biome list, so this is the real rejection case.
        report("wrong", "minecraft:the_void", false) { c, e -> OverworldSpringDecoration.decorate(c, 30303L, e) }
        report("waterOnly", "minecraft:plains", false) { c, e ->
            OverworldSpringDecoration.decorateFeatures(c, 30303L, e, 0..0)
        }
        report("lavaOnly", "minecraft:plains", false) { c, e ->
            OverworldSpringDecoration.decorateFeatures(c, 30303L, e, 1..1)
        }

        val results = StringBuilder()
        for (feature in listOf("spring_water", "spring_lava", "spring_lava_frozen", "nope")) {
            val chunk = pocketedChunk(0, 0, false)
            // (3,-50,1) sits beside exactly one pocket, which is the four-solid-one-hole shape a
            // spring accepts. The pocket itself at (2,-50,1) has five solid sides and is rejected.
            val ok = OverworldSpringDecoration.placeAt(chunk, 3, -50, 1, feature)
            results.append("$feature=$ok/${fold(chunk).second + fold(chunk).third},")
        }
        println("ORACLE direct=$results")
        println("ORACLE ids=" + OverworldSpringDecoration.PLACED_SPRING_IDS.sorted().joinToString(","))
    }
}
