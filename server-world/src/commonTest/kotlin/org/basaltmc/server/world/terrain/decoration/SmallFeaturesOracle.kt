package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the one-block, one-column and one-pile features. */
class SmallFeaturesOracle {
    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    /** Grass over dirt with sand, a dirt path and a stone ceiling, one rule each. */
    private fun groundChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                val worldX = chunkX * 16 + x
                val worldZ = chunkZ * 16 + z
                for (y in 60..69) chunk.setBlockStateId(x, y, z, stateId("dirt"))
                val top =
                    when ((worldX * 5 + worldZ * 3).mod(6)) {
                        0 -> stateId("sand")
                        // A dirt path holds a pile only half the time, which no other block does.
                        1 -> stateId("dirt_path")
                        2 -> stateId("moss_block")
                        else -> Blocks.GRASS_BLOCK.stateId
                    }
                chunk.setBlockStateId(x, 70, z, top)
                // A ceiling four blocks up, which is what a spore blossom hangs from.
                if ((worldX * 7 + worldZ).mod(4) == 0) chunk.setBlockStateId(x, 75, z, stateId("stone"))
            }
        }
        return chunk
    }

    private val ignored = setOf("dirt", "grass_block", "sand", "dirt_path", "moss_block", "stone", "air")

    private fun fold(chunk: Chunk): Pair<Long, Int> {
        var checksum = -3750763034362895579L
        var placed = 0
        for (y in 60..85) {
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

    @Test
    fun dump() {
        val features = SmallFeatures.PLACED_IDS.toList().sorted() + "not_a_feature"
        for (feature in features) {
            var checksum = -3750763034362895579L
            var placed = 0
            val known = StringBuilder()
            var trailing = 0
            for (chunkIndex in 0 until 4) {
                val chunk = groundChunk(chunkIndex, -chunkIndex)
                val random = WorldgenRandom(LegacyRandom(2503L + chunkIndex * 977L))
                // The spore blossom hangs, so it needs air under the ceiling instead of ground.
                val y = if (feature == "spore_blossom") 74 else 71
                val origin = DecorationPos(chunk.chunkX * 16 + 8, y, chunk.chunkZ * 16 + 8)
                known.append(if (SmallFeatures.placeAt(chunk, random, origin, feature)) '1' else '0')
                val (chunkChecksum, chunkPlaced) = fold(chunk)
                checksum = (checksum xor chunkChecksum) * 1099511628211L
                placed += chunkPlaced
                trailing = random.nextInt()
            }
            println("ORACLE $feature known=$known placed=$placed checksum=$checksum after=$trailing")
        }
    }
}
