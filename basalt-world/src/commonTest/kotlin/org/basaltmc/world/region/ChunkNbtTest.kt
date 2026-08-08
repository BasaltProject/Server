package org.basaltmc.world.region

import org.basaltmc.nbt.NbtCompound
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk
import org.basaltmc.world.WorldHeight
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull

class ChunkNbtTest {
    @Test
    fun aSingleBlockSectionRoundTripsWithNoDataArray() {
        val chunk = Chunk(3, -5)
        // Every block in the whole bottom section is stone: a single-entry palette, so `write` must
        // omit "data" entirely (matches vanilla's zero-bit `SingleValuePalette`).
        for (x in 0..15) {
            for (y in WorldHeight.MIN_Y until WorldHeight.MIN_Y + 16) {
                for (z in 0..15) {
                    chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                }
            }
        }

        val tag = ChunkNbt.write(chunk)
        val firstSection = (tag.getList("sections")!![0] as NbtCompound).getCompound("block_states")
        assertEquals(1, firstSection.getList("palette")!!.size)
        assertEquals(false, "data" in firstSection)

        val read = ChunkNbt.read(tag)
        for (x in 0..15) {
            for (z in 0..15) {
                assertEquals(Blocks.STONE.stateId, read.getBlockStateId(x, WorldHeight.MIN_Y, z))
            }
        }
    }

    @Test
    fun aMultiBlockSectionRoundTripsThroughAnIndirectPalette() {
        val chunk = Chunk(0, 0)
        val ids = listOf(Blocks.STONE.stateId, Blocks.DIRT.stateId, Blocks.GRASS_BLOCK.stateId, Blocks.BEDROCK.stateId)
        var i = 0
        for (x in 0..15) {
            for (z in 0..15) {
                chunk.setBlockStateId(x, WorldHeight.MIN_Y, z, ids[i % ids.size])
                i++
            }
        }

        val read = ChunkNbt.read(ChunkNbt.write(chunk))
        i = 0
        for (x in 0..15) {
            for (z in 0..15) {
                assertEquals(ids[i % ids.size], read.getBlockStateId(x, WorldHeight.MIN_Y, z))
                i++
            }
        }
    }

    @Test
    fun everySectionRoundTripsAcrossTheFullWorldHeight() {
        val chunk = Chunk(10, 10)
        for (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y step 7) {
            chunk.setBlockStateId(5, y, 5, Blocks.DIRT.stateId)
        }

        val read = ChunkNbt.read(ChunkNbt.write(chunk))
        for (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y step 7) {
            assertEquals(Blocks.DIRT.stateId, read.getBlockStateId(5, y, 5))
        }
    }

    @Test
    fun basicMetadataRoundTrips() {
        val chunk = Chunk(-42, 17)
        val tag = ChunkNbt.write(chunk)
        assertEquals(-42, tag.getInt("xPos"))
        assertEquals(17, tag.getInt("zPos"))
        assertEquals("minecraft:full", tag.getString("Status"))

        val read = ChunkNbt.read(tag)
        assertEquals(-42, read.chunkX)
        assertEquals(17, read.chunkZ)
    }

    @Test
    fun worldgenDerivedBlockStatesRoundTripWithProperties() {
        val chunk = Chunk(0, 0)
        val oakAxisX = Blocks.OAK_LOG.stateId - 1
        val vineEast = stateId("vine") - 16
        val glowLichenUp = stateId("glow_lichen") - 4
        val sweetBerryAge3 = stateId("sweet_berry_bush") + 3
        val states = listOf(oakAxisX, vineEast, glowLichenUp, sweetBerryAge3)
        states.forEachIndexed { x, stateId -> chunk.setBlockStateId(x, 64, 0, stateId) }

        val tag = ChunkNbt.write(chunk)
        val section = tag.getList("sections")!!.map { it as NbtCompound }.first { it.getByte("Y").toInt() == 4 }
        val palette = section.getCompound("block_states").getList("palette")!!
        val oakEntry =
            assertNotNull(
                palette.map { it as NbtCompound }.firstOrNull { it.getString("Name") == "minecraft:oak_log" },
            )
        assertEquals("x", oakEntry.getCompound("Properties").getString("axis"))

        val read = ChunkNbt.read(tag)
        states.forEachIndexed { x, stateId -> assertEquals(stateId, read.getBlockStateId(x, 64, 0)) }
    }

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")
}
