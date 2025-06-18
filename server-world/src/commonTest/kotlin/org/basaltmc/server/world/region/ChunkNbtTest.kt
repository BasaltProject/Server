package org.basaltmc.server.world.region

import org.basaltmc.server.registry.Biomes
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.ChunkBlockEntity
import org.basaltmc.server.world.ChunkGeneratedEntity
import org.basaltmc.server.world.ChunkHeight
import org.basaltmc.server.world.ChunkScheduledTick
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtType
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull

class ChunkNbtTest {
    /**
     * A chunk holds whatever the world put in it, not only the states someone registered by name.
     *
     * This is the crash a grown sugar cane caused: the palette resolved state ids through the
     * handful of named ones, so the first save after a crop grew, a fence connected or a leaf
     * counted its distance took the whole save thread down with it.
     */
    @Test
    fun statesNobodyNamedStillSaveAndLoad() {
        val chunk = Chunk(0, 0)
        val grown =
            listOf(
                state("sugar_cane", mapOf("age" to "3")),
                state("wheat", mapOf("age" to "5")),
                state("oak_fence", mapOf("north" to "true", "south" to "true", "waterlogged" to "false")),
                state("oak_leaves", mapOf("distance" to "4", "persistent" to "false", "waterlogged" to "false")),
                state("cobblestone_wall", mapOf("up" to "false", "north" to "tall")),
            )
        for ((index, stateId) in grown.withIndex()) chunk.setBlockStateId(index, 70, 0, stateId)

        val read = ChunkNbt.read(ChunkNbt.write(chunk))

        for ((index, stateId) in grown.withIndex()) {
            assertEquals(stateId, read.getBlockStateId(index, 70, 0), "state $stateId did not come back")
        }
    }

    private fun state(
        name: String,
        properties: Map<String, String>,
    ): Int =
        checkNotNull(
            org.basaltmc.server.registry.BlockStateDefinitions
                .stateIdOf(Identifier.vanilla(name), properties),
        )

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
    fun dimensionBoundsAndBiomePalettesRoundTrip() {
        val chunk = Chunk(4, -7, ChunkHeight.NETHER)
        chunk.setBlockStateId(2, 0, 3, Blocks.BEDROCK.stateId)
        chunk.setBlockStateId(2, 255, 3, Blocks.NETHERRACK.stateId)
        chunk.setBiomeId(0, 0, 0, Biomes.NETHER_WASTES.id())
        chunk.setBiomeId(3, 63, 3, Biomes.BASALT_DELTAS.id())

        val tag = ChunkNbt.write(chunk)
        assertEquals(0, tag.getInt("yPos"))
        assertEquals(16, tag.getList("sections")?.size)
        val read = ChunkNbt.read(tag, ChunkHeight.NETHER)

        assertEquals(Blocks.BEDROCK.stateId, read.getBlockStateId(2, 0, 3))
        assertEquals(Blocks.NETHERRACK.stateId, read.getBlockStateId(2, 255, 3))
        assertEquals(Biomes.NETHER_WASTES.id(), read.section(0).getBiomeId(0, 0, 0))
        assertEquals(Biomes.BASALT_DELTAS.id(), read.section(15).getBiomeId(3, 3, 3))
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

    @Test
    fun scheduledTicksRoundTripTheVanillaSavedTickShape() {
        val chunk = Chunk(-2, 3)
        val blockTick = ChunkScheduledTick(Identifier.vanilla("stone"), 15, -12, 4, delay = 7, priority = -1)
        val fluidTick = ChunkScheduledTick(Identifier.vanilla("lava"), 0, 63, 15, delay = 0)
        chunk.scheduleBlockTick(blockTick)
        chunk.scheduleFluidTick(fluidTick)

        val tag = ChunkNbt.write(chunk)
        val writtenBlock = tag.getList("block_ticks")!![0] as NbtCompound
        assertEquals("minecraft:stone", writtenBlock.getString("i"))
        assertEquals(-17, writtenBlock.getInt("x"))
        assertEquals(-12, writtenBlock.getInt("y"))
        assertEquals(52, writtenBlock.getInt("z"))
        assertEquals(7, writtenBlock.getInt("t"))
        assertEquals(-1, writtenBlock.getInt("p"))

        val read = ChunkNbt.read(tag)
        assertEquals(listOf(blockTick), read.scheduledBlockTicks())
        assertEquals(listOf(fluidTick), read.scheduledFluidTicks())
    }

    @Test
    fun generatedEntityDescriptorsRoundTripUntilRuntimeMaterialization() {
        val chunk = Chunk(2, -3)
        val data = NbtCompound().apply { putString("item", "minecraft:elytra") }
        val frame =
            ChunkGeneratedEntity(
                Identifier.vanilla("item_frame"),
                40.03125,
                81.5,
                -38.5,
                270.0f,
                0.0f,
                data,
            )
        chunk.addGeneratedEntity(frame)

        val tag = ChunkNbt.write(chunk)
        val written = tag.getList("basalt_generated_entities")!!.single() as NbtCompound
        assertEquals("minecraft:item_frame", written.getString("id"))
        assertEquals(3, written.getList("Pos")!!.size)

        val read = ChunkNbt.read(tag)
        assertEquals(listOf(frame), read.generatedEntities())
        assertEquals(data, read.generatedEntities().single().data)
    }

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    private fun org.basaltmc.server.registry.Biome.id(): Int =
        Biomes.REGISTRY.idOf(id) ?: error("unregistered biome $id")
}
