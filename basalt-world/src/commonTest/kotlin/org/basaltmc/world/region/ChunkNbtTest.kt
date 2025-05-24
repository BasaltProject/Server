package org.basaltmc.world.region

import org.basaltmc.nbt.NbtCompound
import org.basaltmc.nbt.NbtList
import org.basaltmc.nbt.NbtType
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk
import org.basaltmc.world.ChunkBlockEntity
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
        val cocoaAge2East = stateId("cocoa") + 11
        val paleHangingMossNotTip = stateId("pale_hanging_moss") + 1
        val naturalDormantCreakingHeart = stateId("creaking_heart") + 1
        val hangingMangrovePropaguleAge4 = stateId("mangrove_propagule") + 28
        val eastFourSegmentLeafLitter = stateId("leaf_litter") + 15
        val southEmptyBeeNest = stateId("bee_nest") + 6
        val oakLeavesDistance3 = stateId("oak_leaves") - 16
        val upperSunflower = stateId("sunflower") - 1
        val eastFourWildflowers = stateId("wildflowers") + 15
        val kelpAge23 = stateId("kelp") + 23
        val fourWaterloggedSeaPickles = stateId("sea_pickle") + 6
        val upperTallSeagrass = stateId("tall_seagrass") - 1
        val snowyGrass = stateId("grass_block") - 1
        val eastWaterloggedWallCoral = stateId("tube_coral_wall_fan") + 6
        val generatedMushroomStem = stateId("mushroom_stem") + 34
        val centerBrownMushroomCap = stateId("brown_mushroom_block") + 61
        val states =
            listOf(
                oakAxisX,
                vineEast,
                glowLichenUp,
                sweetBerryAge3,
                cocoaAge2East,
                paleHangingMossNotTip,
                naturalDormantCreakingHeart,
                hangingMangrovePropaguleAge4,
                eastFourSegmentLeafLitter,
                southEmptyBeeNest,
                oakLeavesDistance3,
                upperSunflower,
                eastFourWildflowers,
                kelpAge23,
                fourWaterloggedSeaPickles,
                upperTallSeagrass,
                snowyGrass,
                eastWaterloggedWallCoral,
                generatedMushroomStem,
                centerBrownMushroomCap,
            )
        states.forEachIndexed { index, stateId -> chunk.setBlockStateId(index % 16, 64, index / 16, stateId) }

        val tag = ChunkNbt.write(chunk)
        val section = tag.getList("sections")!!.map { it as NbtCompound }.first { it.getByte("Y").toInt() == 4 }
        val palette = section.getCompound("block_states").getList("palette")!!
        val oakEntry =
            assertNotNull(
                palette.map { it as NbtCompound }.firstOrNull { it.getString("Name") == "minecraft:oak_log" },
            )
        assertEquals("x", oakEntry.getCompound("Properties").getString("axis"))

        val read = ChunkNbt.read(tag)
        states.forEachIndexed { index, stateId ->
            assertEquals(stateId, read.getBlockStateId(index % 16, 64, index / 16))
        }
    }

    @Test
    fun blockEntitiesRoundTripAbsoluteDiskCoordinatesAndCustomData() {
        val chunk = Chunk(-2, 3)
        val beeNest = stateId("bee_nest") + 6
        chunk.setBlockStateId(15, -12, 4, beeNest)
        val entityData = NbtCompound().apply { putString("id", "minecraft:bee") }
        val occupant =
            NbtCompound().apply {
                putCompound("entity_data", entityData)
                putInt("ticks_in_hive", 321)
                putInt("min_ticks_in_hive", 600)
            }
        val customData = NbtCompound().apply { set("bees", NbtList(NbtType.COMPOUND, listOf(occupant))) }
        chunk.setBlockEntity(ChunkBlockEntity(15, -12, 4, Identifier.vanilla("beehive"), customData))

        val tag = ChunkNbt.write(chunk)
        val written = tag.getList("block_entities")!![0] as NbtCompound
        assertEquals("minecraft:beehive", written.getString("id"))
        assertEquals(-17, written.getInt("x"))
        assertEquals(-12, written.getInt("y"))
        assertEquals(52, written.getInt("z"))
        assertEquals(1, written.getList("bees")!!.size)

        val read = ChunkNbt.read(tag)
        val blockEntity = assertNotNull(read.blockEntityAt(15, -12, 4))
        assertEquals(Identifier.vanilla("beehive"), blockEntity.type)
        assertEquals(customData, blockEntity.data)
        assertEquals(false, "id" in blockEntity.data)
        assertEquals(false, "x" in blockEntity.data)
    }

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")
}
