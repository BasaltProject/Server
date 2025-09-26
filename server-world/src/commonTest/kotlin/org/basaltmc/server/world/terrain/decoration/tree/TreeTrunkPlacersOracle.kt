package org.basaltmc.server.world.terrain.decoration.tree

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier
import org.basaltmc.server.world.terrain.decoration.IntProvider
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the seven trunk placers. */
class TreeTrunkPlacersOracle {
    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    /** Grass over stone, plus an obstacle column and a leaf block so the free-space probes can fail. */
    private fun groundChunk(chunkX: Int, chunkZ: Int): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in -64 until 70) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                chunk.setBlockStateId(x, 70, z, stateId("grass_block"))
            }
        }
        for (y in 71..76) chunk.setBlockStateId(11, y, 11, Blocks.STONE.stateId)
        chunk.setBlockStateId(5, 72, 5, stateId("oak_leaves"))
        return chunk
    }

    private fun fold(chunk: Chunk): Pair<Long, Int> {
        var checksum = -3750763034362895579L
        var logs = 0
        for (y in 69..110) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    if (TreeBlockRules.isLogState(id)) logs++
                }
            }
        }
        return checksum to logs
    }

    private val seeds = longArrayOf(11L, 907L, 31337L, 654321L, 8675309L, 42424242L, 987654321L, 5150L)

    private fun report(
        name: String,
        localX: Int,
        localZ: Int,
        placer: TrunkPlacer,
    ) {
        var checksum = -3750763034362895579L
        var logs = 0
        var deferred = 0
        var deferredChecksum = -3750763034362895579L
        val heights = StringBuilder()
        val attached = StringBuilder()
        var trailing = 0
        for ((index, seed) in seeds.withIndex()) {
            val chunk = groundChunk(index * 5 - 30, index * -3 + 12)
            val random = WorldgenRandom(LegacyRandom(seed))
            val originX = chunk.chunkX * 16 + localX
            val originZ = chunk.chunkZ * 16 + localZ
            val treeHeight = placer.getTreeHeight(random)
            heights.append(treeHeight).append(',')
            val attachments =
                placer.placeTrunk(chunk, random, treeHeight, originX, 71, originZ, stateId("oak_log"))
            for (attachment in attachments) {
                attached
                    .append(attachment.x - originX).append(':')
                    .append(attachment.y - 71).append(':')
                    .append(attachment.z - originZ).append(':')
                    .append(attachment.radiusOffset).append(':')
                    .append(if (attachment.doubleTrunk) 1 else 0).append(' ')
            }
            attached.append('|')
            val (chunkChecksum, chunkLogs) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            logs += chunkLogs
            for (write in chunk.takeGeneratedBlockWrites()) {
                deferred++
                for (part in longArrayOf(write.x.toLong(), write.y.toLong(), write.z.toLong(), write.stateId.toLong())) {
                    deferredChecksum = (deferredChecksum xor part) * 1099511628211L
                }
            }
            trailing = random.nextInt()
        }
        println(
            "ORACLE $name heights=$heights logs=$logs deferred=$deferred " +
                "checksum=$checksum deferredChecksum=$deferredChecksum after=$trailing attached=$attached",
        )
    }

    @Test
    fun dump() {
        report("straight", 8, 8, StraightTrunkPlacer(4, 2, 0))
        report("straightEdge", 1, 1, StraightTrunkPlacer(5, 2, 1))
        report("bending", 8, 8, BendingTrunkPlacer(1, 0, 0, 3, IntProvider.uniform(1, 2), stateId("podzol")))
        report("fancy", 8, 8, FancyTrunkPlacer(3, 11, 0))
        report("fancyEdge", 2, 13, FancyTrunkPlacer(3, 11, 0))
        report("forking", 8, 8, ForkingTrunkPlacer(5, 2, 2))
        report("darkOak", 7, 7, DarkOakTrunkPlacer(6, 2, 1))
        report("giant", 7, 7, GiantTrunkPlacer(13, 2, 14))
        report("megaJungle", 7, 7, MegaJungleTrunkPlacer(10, 2, 19))
    }
}
