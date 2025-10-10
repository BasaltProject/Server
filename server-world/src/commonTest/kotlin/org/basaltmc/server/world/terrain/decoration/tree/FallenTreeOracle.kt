package org.basaltmc.server.world.terrain.decoration.tree

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the fallen trees. */
class FallenTreeOracle {
    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    /** Ground with a step and a pit, so the fall probe and the gap rule both get exercised. */
    private fun groundChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                val top = if (x >= 10) 72 else 70
                for (y in -64..top) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                chunk.setBlockStateId(x, top, z, stateId("grass_block"))
            }
        }
        for (x in 4..5) {
            for (z in 6..9) {
                for (y in 66..70) chunk.setBlockStateId(x, y, z, Blocks.AIR.stateId)
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): IntArray {
        var logs = 0
        var vines = 0
        var mushrooms = 0
        for (y in 60..90) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    when (Blocks.byStateId(chunk.getBlockStateId(x, y, z))?.block?.path) {
                        "vine" -> vines++
                        "red_mushroom", "brown_mushroom" -> mushrooms++
                    }
                    if (TreeBlockRules.isLogState(chunk.getBlockStateId(x, y, z))) logs++
                }
            }
        }
        return intArrayOf(logs, vines, mushrooms)
    }

    private fun checksumOf(chunk: Chunk): Long {
        var checksum = -3750763034362895579L
        for (y in 60..90) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    checksum = (checksum xor chunk.getBlockStateId(x, y, z).toLong()) * 1099511628211L
                }
            }
        }
        return checksum
    }

    private val seeds = longArrayOf(11L, 907L, 31337L, 654321L, 8675309L, 42424242L, 987654321L, 5150L)

    private fun report(
        name: String,
        config: FallenTreeConfiguration,
        localX: Int = 7,
        localZ: Int = 8,
        originY: Int = 71,
    ) {
        var checksum = -3750763034362895579L
        var logs = 0
        var vines = 0
        var mushrooms = 0
        var deferred = 0
        val placedFlags = StringBuilder()
        var trailing = 0
        for ((index, seed) in seeds.withIndex()) {
            val chunk = groundChunk(index * 5 - 30, index * -3 + 12)
            val random = WorldgenRandom(LegacyRandom(seed))
            val placed =
                FallenTreeFeature.place(
                    chunk,
                    random,
                    chunk.chunkX * 16 + localX,
                    originY,
                    chunk.chunkZ * 16 + localZ,
                    config,
                )
            placedFlags.append(if (placed) '1' else '0')
            val counts = fold(chunk)
            logs += counts[0]
            vines += counts[1]
            mushrooms += counts[2]
            checksum = (checksum xor checksumOf(chunk)) * 1099511628211L
            deferred += chunk.takeGeneratedBlockWrites().size
            trailing = random.nextInt()
        }
        println(
            "ORACLE $name placed=$placedFlags logs=$logs vines=$vines mushrooms=$mushrooms " +
                "deferred=$deferred checksum=$checksum after=$trailing",
        )
    }

    @Test
    fun dump() {
        report("oak", FallenTreeConfigs.OAK)
        report("oakEdge", FallenTreeConfigs.OAK, localX = 1, localZ = 1)
        report("oakOverPit", FallenTreeConfigs.OAK, localX = 3, localZ = 8)
        report("birch", FallenTreeConfigs.BIRCH)
        report("superBirch", FallenTreeConfigs.SUPER_BIRCH)
        report("spruce", FallenTreeConfigs.SPRUCE)
        report("jungle", FallenTreeConfigs.JUNGLE)
        report("onStep", FallenTreeConfigs.SPRUCE, localX = 9, localZ = 8, originY = 71)
    }
}
