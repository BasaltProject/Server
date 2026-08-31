package org.basaltmc.server.world.terrain.decoration.tree

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.terrain.decoration.IntProvider
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the twelve foliage placers. */
class TreeFoliagePlacersOracle {
    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    /** Air over stone, with a log column to attach to and a water pool so waterlogging can fire. */
    private fun groundChunk(chunkX: Int, chunkZ: Int): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in -64 until 70) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                chunk.setBlockStateId(x, 70, z, stateId("grass_block"))
            }
        }
        for (y in 71..80) chunk.setBlockStateId(8, y, 8, stateId("oak_log"))
        // A pool the crown grows into, so the waterlogged branch of the leaf placement runs.
        for (x in 6..10) {
            for (z in 9..11) chunk.setBlockStateId(x, 78, z, Blocks.WATER.stateId)
        }
        chunk.setBlockStateId(13, 77, 13, stateId("stone"))
        return chunk
    }

    private fun fold(chunk: Chunk): Triple<Long, Int, Int> {
        var checksum = -3750763034362895579L
        var leaves = 0
        var waterlogged = 0
        for (y in 69..110) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    val state = Blocks.byStateId(id)
                    if (state?.block?.path?.endsWith("leaves") == true) {
                        leaves++
                        if (state.properties["waterlogged"] == "true") waterlogged++
                    }
                }
            }
        }
        return Triple(checksum, leaves, waterlogged)
    }

    private val seeds = longArrayOf(11L, 907L, 31337L, 654321L, 8675309L, 42424242L, 987654321L, 5150L)

    private fun report(
        name: String,
        placer: FoliagePlacer,
        doubleTrunk: Boolean = false,
        radiusOffset: Int = 0,
        treeHeight: Int = 8,
        localX: Int = 8,
        localZ: Int = 8,
    ) {
        var checksum = -3750763034362895579L
        var leaves = 0
        var waterlogged = 0
        var deferred = 0
        var tracked = 0
        val shape = StringBuilder()
        var trailing = 0
        for ((index, seed) in seeds.withIndex()) {
            val chunk = groundChunk(index * 5 - 30, index * -3 + 12)
            val random = WorldgenRandom(LegacyRandom(seed))
            val attachment =
                FoliageAttachment(
                    chunk.chunkX * 16 + localX,
                    79,
                    chunk.chunkZ * 16 + localZ,
                    radiusOffset,
                    doubleTrunk,
                )
            val leafRadius = placer.foliageRadius(random, treeHeight)
            val foliageHeight = placer.foliageHeight(random, treeHeight)
            val offset = placer.offset(random)
            shape.append(leafRadius).append(':').append(foliageHeight).append(':').append(offset).append(',')
            val positions = mutableSetOf<Triple<Int, Int, Int>>()
            placer.createFoliage(
                chunk,
                random,
                stateId("oak_leaves"),
                attachment,
                foliageHeight,
                leafRadius,
                offset,
                positions,
            )
            tracked += positions.size
            val (chunkChecksum, chunkLeaves, chunkWaterlogged) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            leaves += chunkLeaves
            waterlogged += chunkWaterlogged
            deferred += chunk.takeGeneratedBlockWrites().size
            trailing = random.nextInt()
        }
        println(
            "ORACLE $name leaves=$leaves waterlogged=$waterlogged tracked=$tracked deferred=$deferred " +
                "checksum=$checksum after=$trailing shape=$shape",
        )
    }

    @Test
    fun dump() {
        val two = IntProvider.of(2)
        val zero = IntProvider.of(0)
        report("blob", BlobFoliagePlacer(two, zero, 3))
        report("blobDouble", BlobFoliagePlacer(two, zero, 3), doubleTrunk = true, radiusOffset = 1)
        report("bush", BushFoliagePlacer(two, zero, 3))
        report("megaJungle", MegaJungleFoliagePlacer(two, zero, 2))
        report("megaJungleDouble", MegaJungleFoliagePlacer(two, zero, 2), doubleTrunk = true)
        report("fancy", FancyFoliagePlacer(two, IntProvider.of(4), 0))
        report("acacia", AcaciaFoliagePlacer(IntProvider.of(2), IntProvider.of(0)))
        report("spruce", SpruceFoliagePlacer(two, IntProvider.uniform(1, 2), IntProvider.uniform(1, 2)))
        report("pine", PineFoliagePlacer(IntProvider.of(1), IntProvider.of(1), IntProvider.uniform(3, 4)))
        report("darkOak", DarkOakFoliagePlacer(IntProvider.of(1), zero))
        report("darkOakDouble", DarkOakFoliagePlacer(IntProvider.of(1), zero), doubleTrunk = true)
        report("megaPine", MegaPineFoliagePlacer(zero, zero, IntProvider.uniform(13, 17)), doubleTrunk = true)
        report(
            "randomSpread",
            RandomSpreadFoliagePlacer(IntProvider.of(3), IntProvider.of(0), IntProvider.of(2), 70),
        )
        report(
            "weightedSpread",
            WeightedRandomSpreadFoliagePlacer(
                IntProvider.of(3),
                IntProvider.of(0),
                IntProvider.of(2),
                50,
                listOf(stateId("azalea_leaves") to 3, stateId("flowering_azalea_leaves") to 1),
            ),
        )
        report(
            "cherry",
            CherryFoliagePlacer(
                IntProvider.of(4),
                IntProvider.of(0),
                IntProvider.of(5),
                0.25f,
                0.5f,
                0.25f,
                0.25f,
            ),
        )
        report(
            "cherryEdge",
            CherryFoliagePlacer(
                IntProvider.of(4),
                IntProvider.of(0),
                IntProvider.of(5),
                0.25f,
                0.5f,
                0.25f,
                0.25f,
            ),
            localX = 1,
            localZ = 14,
        )
    }
}
