package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class LushVegetationPatchFeatureTest {
    @Test
    fun floorMossReplacesTheGroundAndDistributesVegetationAboveIt() {
        val chunk = caveChunk()
        LushVegetationPatchFeature.placeFloorMoss(
            chunk,
            WorldgenRandom(XoroshiroRandom(1234L)),
            DecorationPos(8, FLOOR_Y + 1, 8),
        )

        assertTrue(countBlock(chunk, FLOOR_Y, "moss_block") > 0)
        assertTrue(
            listOf("flowering_azalea", "azalea", "moss_carpet", "short_grass", "tall_grass")
                .sumOf { path -> countBlock(chunk, FLOOR_Y + 1, path) } > 0,
        )
    }

    @Test
    fun ceilingMossReplacesTheCeilingAndHangsCaveVinesBelowIt() {
        var foundVines = false
        for (seed in 0L..100L) {
            val chunk = caveChunk()
            LushVegetationPatchFeature.placeCeilingMoss(
                chunk,
                WorldgenRandom(XoroshiroRandom(seed)),
                DecorationPos(8, CEILING_Y - 1, 8),
            )
            assertTrue(countBlock(chunk, CEILING_Y, "moss_block") > 0)
            foundVines =
                (FLOOR_Y + 1 until CEILING_Y).any { y ->
                    countBlock(chunk, y, "cave_vines") + countBlock(chunk, y, "cave_vines_plant") > 0
                }
            if (foundVines) break
        }
        assertTrue(foundVines, "expected sampled ceiling moss to grow cave vines")
    }

    @Test
    fun clayPatchReplacesLushGroundAtTheScannedFloor() {
        val chunk = caveChunk()
        LushVegetationPatchFeature.placeClay(
            chunk,
            WorldgenRandom(XoroshiroRandom(3L)),
            DecorationPos(8, FLOOR_Y + 1, 8),
        )
        assertTrue(countBlock(chunk, FLOOR_Y, "clay") > 0)
    }

    @Test
    fun smallDripleafCopiesWaterloggingPerHalfAndChecksItsSupport() {
        val seed = seedSelectingDripleaf(0)
        val origin = DecorationPos(8, FLOOR_Y + 1, 8)
        val supported = Chunk(0, 0)
        supported.setBlockStateId(8, FLOOR_Y, 8, stateId("clay"))
        supported.setBlockStateId(8, FLOOR_Y + 1, 8, Blocks.WATER.stateId)

        LushVegetationPatchFeature.placeDripleafAt(
            supported,
            WorldgenRandom(XoroshiroRandom(seed)),
            origin,
        )

        val lower = checkNotNull(Blocks.byStateId(supported.getBlockStateId(8, FLOOR_Y + 1, 8)))
        val upper = checkNotNull(Blocks.byStateId(supported.getBlockStateId(8, FLOOR_Y + 2, 8)))
        assertEquals("small_dripleaf", lower.block.path)
        assertEquals("lower", lower.properties["half"])
        assertEquals("true", lower.properties["waterlogged"])
        assertEquals("small_dripleaf", upper.block.path)
        assertEquals(lower.properties["facing"], upper.properties["facing"])
        assertEquals("upper", upper.properties["half"])
        assertEquals("false", upper.properties["waterlogged"])

        val unsupported = Chunk(0, 0)
        unsupported.setBlockStateId(8, FLOOR_Y, 8, Blocks.STONE.stateId)
        LushVegetationPatchFeature.placeDripleafAt(
            unsupported,
            WorldgenRandom(XoroshiroRandom(seed)),
            origin,
        )
        assertTrue(Blocks.isAir(unsupported.getBlockStateId(8, FLOOR_Y + 1, 8)))
    }

    @Test
    fun bigDripleafAcceptsEveryWaterStateButKeepsItsConfiguredDryState() {
        val seed = seedSelectingDripleaf(1)
        val chunk = Chunk(0, 0)
        val origin = DecorationPos(8, FLOOR_Y + 1, 8)
        chunk.setBlockStateId(8, FLOOR_Y + 1, 8, Blocks.WATER.stateId)
        chunk.setBlockStateId(8, FLOOR_Y + 2, 8, Blocks.WATER.stateId + 1)

        LushVegetationPatchFeature.placeDripleafAt(
            chunk,
            WorldgenRandom(XoroshiroRandom(seed)),
            origin,
        )

        val placed = checkNotNull(Blocks.byStateId(chunk.getBlockStateId(8, FLOOR_Y + 1, 8)))
        assertTrue(placed.block.path == "big_dripleaf" || placed.block.path == "big_dripleaf_stem")
        assertEquals("false", placed.properties["waterlogged"])
    }

    @Test
    fun waterloggedClayPatchPromotesTheDripleafAtThePoolSurface() {
        var placedStates = emptyList<Int>()
        for (seed in 0L..100L) {
            val selector = WorldgenRandom(XoroshiroRandom(seed))
            if (selector.nextBoolean()) continue
            val chunk = caveChunk()
            LushVegetationPatchFeature.placeClay(
                chunk,
                WorldgenRandom(XoroshiroRandom(seed)),
                DecorationPos(8, FLOOR_Y + 1, 8),
            )
            placedStates = dripleafStates(chunk)
            if (placedStates.isNotEmpty()) break
        }

        assertTrue(placedStates.isNotEmpty(), "expected a waterlogged clay patch to select dripleaf")
        assertTrue(placedStates.any(Blocks::hasWaterFluid))
    }

    private fun caveChunk(): Chunk =
        Chunk(0, 0).also { chunk ->
            for (x in 0 until 16) {
                for (z in 0 until 16) {
                    chunk.setBlockStateId(x, FLOOR_Y - 1, z, Blocks.STONE.stateId)
                    chunk.setBlockStateId(x, FLOOR_Y, z, Blocks.STONE.stateId)
                    chunk.setBlockStateId(x, CEILING_Y, z, Blocks.STONE.stateId)
                    chunk.setBlockStateId(x, CEILING_Y + 1, z, Blocks.STONE.stateId)
                }
            }
        }

    private fun countBlock(
        chunk: Chunk,
        y: Int,
        path: String,
    ): Int {
        val block = Identifier.vanilla(path)
        var count = 0
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                if (Blocks.byStateId(chunk.getBlockStateId(x, y, z))?.block == block) count++
            }
        }
        return count
    }

    private fun seedSelectingDripleaf(index: Int): Long =
        (0L..1_000L).first { seed -> WorldgenRandom(XoroshiroRandom(seed)).nextInt(5) == index }

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    private fun dripleafStates(chunk: Chunk): List<Int> =
        buildList {
            for (x in 0 until 16) {
                for (z in 0 until 16) {
                    for (y in FLOOR_Y - 1..FLOOR_Y + 6) {
                        val stateId = chunk.getBlockStateId(x, y, z)
                        val path = Blocks.byStateId(stateId)?.block?.path
                        if (path?.contains("dripleaf") == true) add(stateId)
                    }
                }
            }
        }

    private companion object {
        const val FLOOR_Y = 30
        const val CEILING_Y = 50
    }
}
