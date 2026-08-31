package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the shared pumpkin, cane, cactus and firefly features. */
class ExtraVegetationOracle {
    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    /** Grass and sand shores around a pond, which is what cane and fireflies need beside them. */
    private fun shoreChunk(chunkX: Int, chunkZ: Int, surface: String): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 66 until 70) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                chunk.setBlockStateId(x, 70, z, stateId(surface))
            }
        }
        for (x in 5..10) {
            for (z in 5..10) {
                chunk.setBlockStateId(x, 70, z, Blocks.WATER.stateId)
                chunk.setBlockStateId(x, 69, z, stateId("sand"))
            }
        }
        return chunk
    }

    private fun counts(chunk: Chunk): IntArray {
        var pumpkins = 0
        var cane = 0
        var cactus = 0
        var bushes = 0
        for (y in 66..100) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    when (Blocks.byStateId(chunk.getBlockStateId(x, y, z))?.block?.path) {
                        "pumpkin" -> pumpkins++
                        "sugar_cane" -> cane++
                        "cactus", "cactus_flower" -> cactus++
                        "firefly_bush" -> bushes++
                    }
                }
            }
        }
        return intArrayOf(pumpkins, cane, cactus, bushes)
    }

    private fun checksumOf(chunk: Chunk): Long {
        var checksum = -3750763034362895579L
        for (y in 66..100) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    checksum = (checksum xor chunk.getBlockStateId(x, y, z).toLong()) * 1099511628211L
                }
            }
        }
        return checksum
    }

    private fun report(
        name: String,
        decoration: BiomeVegetalDecoration,
        surface: String = "grass_block",
        count: Int = 60,
    ) {
        var checksum = -3750763034362895579L
        val totals = intArrayOf(0, 0, 0, 0)
        var trailing = 0
        for (index in 0 until count) {
            val chunk = shoreChunk(index * 5 - 30, index * -3 + 12, surface)
            val random = WorldgenRandom(LegacyRandom(31L + index * 7919L))
            decoration.decorate(chunk, random)
            val chunkCounts = counts(chunk)
            for (i in totals.indices) totals[i] += chunkCounts[i]
            checksum = (checksum xor checksumOf(chunk)) * 1099511628211L
            trailing = random.nextInt()
        }
        println(
            "ORACLE $name pumpkins=${totals[0]} cane=${totals[1]} cactus=${totals[2]} bushes=${totals[3]} " +
                "checksum=$checksum after=$trailing",
        )
    }

    @Test
    fun dump() {
        // Pumpkin is 1/300, so it needs far more chunks than the rest to fire at all.
        report("pumpkinOnly", ExtraVegetationDecoration.PUMPKIN_ONLY, count = 900)
        report("standard", ExtraVegetationDecoration.DEFAULT)
        report("standardWithoutPumpkin", ExtraVegetationDecoration.DEFAULT_WITHOUT_PUMPKIN)
        report("fireflyNearWater", ExtraVegetationDecoration.FIREFLY_NEAR_WATER)
        report("desert", ExtraVegetationDecoration.DESERT, surface = "sand")
        report("desertWithoutPumpkin", ExtraVegetationDecoration.DESERT_WITHOUT_PUMPKIN, surface = "sand")
        report(
            "desertWithoutPumpkinOrCactus",
            ExtraVegetationDecoration.DESERT_WITHOUT_PUMPKIN_OR_CACTUS,
            surface = "sand",
        )
        report("badlands", ExtraVegetationDecoration.BADLANDS, surface = "red_sand")
        report("badlandsWithoutPumpkin", ExtraVegetationDecoration.BADLANDS_WITHOUT_PUMPKIN, surface = "red_sand")
        report(
            "badlandsWithoutPumpkinOrCactus",
            ExtraVegetationDecoration.BADLANDS_WITHOUT_PUMPKIN_OR_CACTUS,
            surface = "red_sand",
        )
        report("swamp", ExtraVegetationDecoration.SWAMP)
        report("swampWithoutPumpkin", ExtraVegetationDecoration.SWAMP_WITHOUT_PUMPKIN)
        report("swampFireflies", ExtraVegetationDecoration.SWAMP_FIREFLIES)
        report("mangrove", ExtraVegetationDecoration.MANGROVE)
        // Stone carries none of these plants, so every survival check rejects.
        report("bareStone", ExtraVegetationDecoration.DEFAULT, surface = "stone")
    }
}
