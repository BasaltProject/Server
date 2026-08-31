package org.basaltmc.server.world.terrain.decoration.tree

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.terrain.decoration.IntProvider
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the mangrove and cherry placers and the mangrove roots. */
class TreeBranchingTrunkPlacersOracle {
    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    /** Grass over stone, with an obstacle column so the trunk probes can fail. */
    private fun groundChunk(chunkX: Int, chunkZ: Int): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in -64 until 70) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                chunk.setBlockStateId(x, 70, z, stateId("grass_block"))
            }
        }
        for (y in 71..78) chunk.setBlockStateId(11, y, 11, Blocks.STONE.stateId)
        return chunk
    }

    /** A mangrove swamp: mud under shallow water, which is what the roots grow through. */
    private fun swampChunk(chunkX: Int, chunkZ: Int): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in -64 until 66) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                for (y in 66..70) chunk.setBlockStateId(x, y, z, stateId("mud"))
                for (y in 71..73) chunk.setBlockStateId(x, y, z, Blocks.WATER.stateId)
            }
        }
        for (y in 66..73) chunk.setBlockStateId(12, y, 12, Blocks.STONE.stateId)
        return chunk
    }

    private fun fold(chunk: Chunk): Triple<Long, Int, Int> {
        var checksum = -3750763034362895579L
        var logs = 0
        var roots = 0
        for (y in 60..110) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    if (TreeBlockRules.isLogState(id)) logs++
                    val path = Blocks.byStateId(id)?.block?.path
                    if (path == "mangrove_roots" || path == "muddy_mangrove_roots" || path == "moss_carpet") roots++
                }
            }
        }
        return Triple(checksum, logs, roots)
    }

    private val seeds = longArrayOf(11L, 907L, 31337L, 654321L, 8675309L, 42424242L, 987654321L, 5150L)

    private fun report(
        name: String,
        localX: Int,
        localZ: Int,
        swamp: Boolean,
        body: (Chunk, WorldgenRandom, Int, Int) -> String,
    ) {
        var checksum = -3750763034362895579L
        var logs = 0
        var roots = 0
        var deferred = 0
        val detail = StringBuilder()
        var trailing = 0
        for ((index, seed) in seeds.withIndex()) {
            val chunk = if (swamp) swampChunk(index * 5 - 30, index * -3 + 12) else groundChunk(index * 5 - 30, index * -3 + 12)
            val random = WorldgenRandom(LegacyRandom(seed))
            detail.append(body(chunk, random, chunk.chunkX * 16 + localX, chunk.chunkZ * 16 + localZ)).append('|')
            val (chunkChecksum, chunkLogs, chunkRoots) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            logs += chunkLogs
            roots += chunkRoots
            deferred += chunk.takeGeneratedBlockWrites().size
            trailing = random.nextInt()
        }
        println(
            "ORACLE $name logs=$logs roots=$roots deferred=$deferred checksum=$checksum " +
                "after=$trailing detail=$detail",
        )
    }

    private fun trunkReport(
        name: String,
        localX: Int,
        localZ: Int,
        originY: Int,
        swamp: Boolean,
        placer: TrunkPlacer,
    ) = report(name, localX, localZ, swamp) { chunk, random, originX, originZ ->
        val treeHeight = placer.getTreeHeight(random)
        val attachments = placer.placeTrunk(chunk, random, treeHeight, originX, originY, originZ, stateId("mangrove_log"))
        buildString {
            append(treeHeight).append(';')
            for (a in attachments) {
                append(a.x - originX).append(':').append(a.y - originY).append(':')
                    .append(a.z - originZ).append(':').append(a.radiusOffset).append(' ')
            }
        }
    }

    @Test
    fun dump() {
        // parity: the real mangrove and cherry trunk placer arguments.
        val mangrove = UpwardsBranchingTrunkPlacer(2, 1, 4, IntProvider.uniform(1, 4), 0.5f, IntProvider.uniform(0, 1))
        trunkReport("mangroveTrunk", 8, 8, 74, swamp = true, placer = mangrove)
        trunkReport("mangroveTrunkGround", 8, 8, 71, swamp = false, placer = mangrove)
        val cherry =
            CherryTrunkPlacer(
                7,
                1,
                0,
                IntProvider.uniform(1, 3),
                IntProvider.uniform(2, 4),
                IntProvider.Uniform(-4, -3),
                IntProvider.uniform(-1, 0),
            )
        trunkReport("cherryTrunk", 8, 8, 71, swamp = false, placer = cherry)
        trunkReport("cherryTrunkEdge", 2, 13, 71, swamp = false, placer = cherry)

        val roots = MangroveRootPlacer(IntProvider.uniform(1, 3))
        report("mangroveRoots", 8, 8, swamp = true) { chunk, random, originX, originZ ->
            val trunkOriginY = roots.trunkOriginY(random, 71)
            val placed = roots.placeRoots(chunk, random, originX, 71, originZ, trunkOriginY)
            "$trunkOriginY:$placed"
        }
        report("mangroveRootsBlocked", 12, 12, swamp = true) { chunk, random, originX, originZ ->
            val trunkOriginY = roots.trunkOriginY(random, 71)
            val placed = roots.placeRoots(chunk, random, originX, 71, originZ, trunkOriginY)
            "$trunkOriginY:$placed"
        }
    }
}
