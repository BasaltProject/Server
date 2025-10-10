package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the ocean floor vegetal entries. */
class AquaticVegetalOracle {
    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    /** A rolling floor under deep water, with magma and a slab so both support rules matter. */
    private fun oceanChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                val worldX = chunkX * 16 + x
                val worldZ = chunkZ * 16 + z
                // A shallow shelf under a third of the columns, where a kelp stalk runs out of water
                // after a block or two instead of always having room for its full height.
                val shallow = (worldX * 11 + worldZ * 3).mod(3) == 0
                val floor = if (shallow) 60 else 40 + (worldX * 3 + worldZ * 7).mod(5)
                for (y in 30..floor) chunk.setBlockStateId(x, y, z, stateId("gravel"))
                val top =
                    when ((worldX * 5 + worldZ).mod(9)) {
                        // Magma holds nothing at all, and a slab is the shape whose top edge alone
                        // supports a sea pickle.
                        0 -> stateId("magma_block")
                        1 -> stateId("smooth_stone_slab")
                        else -> stateId("sand")
                    }
                chunk.setBlockStateId(x, floor, z, top)
                for (y in floor + 1..62) chunk.setBlockStateId(x, y, z, Blocks.WATER.stateId)
            }
        }
        return chunk
    }

    private val counted = setOf("seagrass", "tall_seagrass", "kelp", "kelp_plant", "sea_pickle")

    private fun fold(chunk: Chunk): Pair<Long, Int> {
        var checksum = -3750763034362895579L
        var placed = 0
        for (y in 30..70) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    if (Blocks.byStateId(id)?.block?.path in counted) placed++
                }
            }
        }
        return checksum to placed
    }

    private fun report(
        name: String,
        count: Int,
        step: BiomeVegetalDecoration,
    ) {
        var checksum = -3750763034362895579L
        var placed = 0
        var deferred = 0
        var after = 0
        for (chunkIndex in 0 until count) {
            val chunk = oceanChunk(chunkIndex * 2 - 7, chunkIndex * -3 + 4)
            val random = WorldgenRandom(XoroshiroRandom(6199L + chunkIndex))
            step.decorate(chunk, random)
            val (chunkChecksum, chunkPlaced) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            placed += chunkPlaced
            deferred += chunk.takeGeneratedBlockWrites().size
            after = random.nextInt()
        }
        println("ORACLE $name placed=$placed deferred=$deferred checksum=$checksum after=$after")
    }

    @Test
    fun dump() {
        report("ocean", 6, AquaticDecoration.OCEAN)
        report("deepOcean", 6, AquaticDecoration.DEEP_OCEAN)
        report("coldOcean", 6, AquaticDecoration.COLD_OCEAN)
        report("deepColdOcean", 6, AquaticDecoration.DEEP_COLD_OCEAN)
        report("lukewarmOcean", 6, AquaticDecoration.LUKEWARM_OCEAN)
        report("deepLukewarmOcean", 6, AquaticDecoration.DEEP_LUKEWARM_OCEAN)
        report("river", 6, AquaticDecoration.RIVER)
        report("swamp", 6, AquaticDecoration.SWAMP)
        report("warmOcean", 24, AquaticDecoration.WARM_OCEAN)

        var checksum = -3750763034362895579L
        var placed = 0
        val known = StringBuilder()
        var trailing = 0
        val features = AquaticDecoration.PLACED_IDS.toList().sorted() + "not_a_feature"
        for (feature in features) {
            for (chunkIndex in 0 until 3) {
                val chunk = oceanChunk(chunkIndex, -chunkIndex)
                val random = WorldgenRandom(LegacyRandom(797L + chunkIndex * 3821L))
                val origin = DecorationPos(chunk.chunkX * 16 + 8, 46 + chunkIndex, chunk.chunkZ * 16 + 8)
                known.append(if (AquaticDecoration.placeAt(chunk, random, origin, feature)) '1' else '0')
                val (chunkChecksum, chunkPlaced) = fold(chunk)
                checksum = (checksum xor chunkChecksum) * 1099511628211L
                placed += chunkPlaced
                trailing = random.nextInt()
            }
        }
        println("ORACLE placeAt known=$known placed=$placed checksum=$checksum after=$trailing")
    }
}
