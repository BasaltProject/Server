package org.basaltmc.server.world.terrain

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test

/** The frozen Kotlin as the oracle for where a buried treasure sinks to and what it leaves. */
class BuriedTreasureGeneratorOracle {
    private fun state(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    /**
     * Sandstone on stone under a sea cave: the chest has to walk down past the hollow before it
     * finds a floor it accepts, and the shell then has open sides to fill.
     */
    private fun beachChunk(
        chunkX: Int,
        chunkZ: Int,
        hollow: Boolean,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        val sandstone = state("sandstone")
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in -16..48) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                for (y in 49..52) chunk.setBlockStateId(x, y, z, sandstone)
                for (y in 53..62) chunk.setBlockStateId(x, y, z, Blocks.WATER.stateId)
                // A pocket of water under the sandstone, so the walk down has something to skip.
                if (hollow) for (y in 44..48) chunk.setBlockStateId(x, y, z, Blocks.WATER.stateId)
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Triple<Long, Int, Int> {
        var checksum = -3750763034362895579L
        var written = 0
        for (y in -16..66) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    if (id != Blocks.STONE.stateId && id != Blocks.WATER.stateId && id != Blocks.AIR.stateId) {
                        written++
                    }
                }
            }
        }
        // The loot seed is the only thing the structure's index in its step reaches the world
        // through, so it goes into the checksum rather than just the count.
        for (entity in chunk.blockEntities()) {
            checksum = (checksum xor entity.data.getLong("LootTableSeed", 0L)) * 1099511628211L
        }
        return Triple(checksum, written, chunk.blockEntities().size)
    }

    private fun report(
        name: String,
        seed: Long,
        biome: String,
        hollow: Boolean,
        chunks: Int,
    ) {
        val generator =
            BuriedTreasureStructureGenerator(
                biomeAt = { _, _, _ -> biome },
                beachBiomes = setOf("minecraft:beach", "minecraft:snowy_beach"),
                surfaceHeightAt = { _, _ -> 53 },
            )
        var checksum = -3750763034362895579L
        var written = 0
        var blockEntities = 0
        var starts = 0
        for (index in 0 until chunks) {
            val chunkX = index % 20 - 10
            val chunkZ = index / 20 - 10
            val chunk = beachChunk(chunkX, chunkZ, hollow)
            generator.generateInto(chunk, seed)
            val (chunkChecksum, chunkWritten, chunkBlockEntities) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            written += chunkWritten
            blockEntities += chunkBlockEntities
            starts += chunk.structures.starts().size
        }
        println("ORACLE $name written=$written blockEntities=$blockEntities starts=$starts checksum=$checksum")
    }

    @Test
    fun dump() {
        report("beach", 606L, "minecraft:beach", hollow = false, chunks = 400)
        // The same seed over a sea cave, so the chest sinks past it to the stone below.
        report("hollow", 606L, "minecraft:beach", hollow = true, chunks = 400)
        // A biome outside the tag leaves every chunk alone.
        report("plains", 606L, "minecraft:plains", hollow = false, chunks = 400)
        report("otherSeed", 77L, "minecraft:beach", hollow = false, chunks = 400)
    }
}
