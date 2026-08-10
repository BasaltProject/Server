package org.basaltmc.world.structure

import org.basaltmc.nbt.NbtCompound
import org.basaltmc.nbt.NbtInt
import org.basaltmc.nbt.NbtList
import org.basaltmc.nbt.NbtType
import org.basaltmc.random.WorldgenRandom
import org.basaltmc.random.XoroshiroRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk
import kotlin.test.Test
import kotlin.test.assertEquals

/** Synthetic NBT built by hand, matching the real `size`/`palette`/`blocks` structure format, not extracted from any Mojang file. */
private fun syntheticTemplateNbt(): NbtCompound {
    val root = NbtCompound()
    root["size"] = NbtList(NbtType.INT, listOf(NbtInt(2), NbtInt(1), NbtInt(1)))

    val paletteEntry =
        NbtCompound().apply {
            putString("Name", "minecraft:stone")
        }
    root["palette"] = NbtList(NbtType.COMPOUND, listOf(paletteEntry))

    val block0 =
        NbtCompound().apply {
            set("pos", NbtList(NbtType.INT, listOf(NbtInt(0), NbtInt(0), NbtInt(0))))
            putInt("state", 0)
        }
    val block1 =
        NbtCompound().apply {
            set("pos", NbtList(NbtType.INT, listOf(NbtInt(1), NbtInt(0), NbtInt(0))))
            putInt("state", 0)
        }
    root["blocks"] = NbtList(NbtType.COMPOUND, listOf(block0, block1))
    return root
}

class StructureTemplateTest {
    @Test
    fun retainsExactPalettePropertiesAndRotatesRegisteredFacings() {
        val root = NbtCompound()
        root["size"] = NbtList(NbtType.INT, listOf(NbtInt(2), NbtInt(1), NbtInt(1)))
        root["palette"] =
            NbtList(
                NbtType.COMPOUND,
                listOf(
                    NbtCompound().apply {
                        putString("Name", "minecraft:chest")
                        this["Properties"] =
                            NbtCompound().apply {
                                putString("type", "single")
                                putString("facing", "east")
                                putString("waterlogged", "false")
                            }
                    },
                    NbtCompound().apply {
                        putString("Name", "minecraft:lava")
                        this["Properties"] = NbtCompound().apply { putString("level", "8") }
                    },
                ),
            )
        root["blocks"] = NbtList(NbtType.COMPOUND, listOf(blockEntry(0, 0), blockEntry(1, 1)))
        val template = StructureTemplate().apply { load(root) }
        val chunk = Chunk(0, 0)

        assertEquals("east", template.blocks[0].properties["facing"])
        assertEquals(110, template.blocks[1].stateId)
        template.placeInto(
            chunk,
            StructurePos(4, 64, 4),
            StructureRotation.CLOCKWISE_90,
            StructureMirror.NONE,
            StructurePos(0, 0, 0),
            StructureBoundingBox(4, 64, 4, 4, 64, 5),
        )

        assertEquals(3994, chunk.getBlockStateId(4, 64, 4))
        assertEquals(110, chunk.getBlockStateId(4, 64, 5))
    }

    @Test
    fun preservesBlockEntitiesAndConsumesLootSeedsOnlyForContainers() {
        val root = NbtCompound()
        root["size"] = NbtList(NbtType.INT, listOf(NbtInt(2), NbtInt(1), NbtInt(1)))
        root["palette"] =
            NbtList(
                NbtType.COMPOUND,
                listOf(
                    NbtCompound().apply { putString("Name", "minecraft:chest") },
                    NbtCompound().apply { putString("Name", "minecraft:spawner") },
                ),
            )
        root["blocks"] =
            NbtList(
                NbtType.COMPOUND,
                listOf(
                    blockEntry(0, 0).apply {
                        this["nbt"] =
                            NbtCompound().apply {
                                putString("id", "minecraft:chest")
                                putString("LootTable", "test:chests/template")
                            }
                    },
                    blockEntry(1, 1).apply {
                        this["nbt"] =
                            NbtCompound().apply {
                                putString("id", "minecraft:mob_spawner")
                                putString("test_marker", "preserved")
                            }
                    },
                ),
            )
        val template = StructureTemplate().apply { load(root) }
        val chunk = Chunk(0, 0)
        val random = WorldgenRandom(XoroshiroRandom(91L))
        val oracle = WorldgenRandom(XoroshiroRandom(91L))
        val expectedLootSeed = oracle.nextLong()

        template.placeInto(
            chunk,
            StructurePos(4, 64, 4),
            StructureRotation.NONE,
            StructureMirror.NONE,
            StructurePos(0, 0, 0),
            StructureBoundingBox(4, 64, 4, 5, 64, 4),
            random = random,
        )

        val chest = chunk.blockEntityAt(4, 64, 4)!!
        assertEquals(Identifier.vanilla("chest"), chest.type)
        assertEquals("test:chests/template", chest.data.getString("LootTable"))
        assertEquals(expectedLootSeed, chest.data.getLong("LootTableSeed"))
        assertEquals(0, chest.data.getCompound("components").size)
        val spawner = chunk.blockEntityAt(5, 64, 4)!!
        assertEquals(Identifier.vanilla("mob_spawner"), spawner.type)
        assertEquals("preserved", spawner.data.getString("test_marker"))
        assertEquals(0, spawner.data.getCompound("components").size)
        assertEquals(oracle.nextLong(), random.nextLong())
    }

    @Test
    fun singlePoolElementsApplyTheirExactBlockIgnoreProcessor() {
        val root = NbtCompound()
        root["size"] = NbtList(NbtType.INT, listOf(NbtInt(2), NbtInt(1), NbtInt(1)))
        root["palette"] =
            NbtList(
                NbtType.COMPOUND,
                listOf(
                    NbtCompound().apply { putString("Name", "minecraft:air") },
                    NbtCompound().apply { putString("Name", "minecraft:structure_block") },
                ),
            )
        root["blocks"] = NbtList(NbtType.COMPOUND, listOf(blockEntry(0, 0), blockEntry(1, 1)))
        val template = StructureTemplate().apply { load(root) }
        val loader: TemplateLoader = { template }
        val stone = Blocks.REGISTRY.byIdentifier(Identifier.vanilla("stone"))!!.stateId
        val legacyChunk =
            Chunk(0, 0).apply {
                setBlockStateId(0, 64, 0, stone)
                setBlockStateId(1, 64, 0, stone)
            }
        val modernChunk =
            Chunk(0, 0).apply {
                setBlockStateId(0, 64, 0, stone)
                setBlockStateId(1, 64, 0, stone)
            }
        val clip = StructureBoundingBox(0, 64, 0, 1, 64, 0)
        val legacy =
            LegacySinglePoolElement(
                "test:air",
                StructureProcessorList.EMPTY,
                StructureProjection.RIGID,
                ignoreAir = true,
            )
        val modern =
            LegacySinglePoolElement(
                "test:air",
                StructureProcessorList.EMPTY,
                StructureProjection.RIGID,
                ignoreAir = false,
            )

        legacy.place(
            loader,
            legacyChunk,
            StructurePos(0, 64, 0),
            StructureRotation.NONE,
            clip,
            WorldgenRandom(XoroshiroRandom(1L)),
        )
        modern.place(
            loader,
            modernChunk,
            StructurePos(0, 64, 0),
            StructureRotation.NONE,
            clip,
            WorldgenRandom(XoroshiroRandom(1L)),
        )

        assertEquals(stone, legacyChunk.getBlockStateId(0, 64, 0))
        assertEquals(stone, legacyChunk.getBlockStateId(1, 64, 0))
        assertEquals(Blocks.AIR.stateId, modernChunk.getBlockStateId(0, 64, 0))
        assertEquals(stone, modernChunk.getBlockStateId(1, 64, 0))
    }

    @Test
    fun jigsawFinalStatesKeepPropertiesAndStructureVoidLeavesTheWorldUntouched() {
        val root = NbtCompound()
        root["size"] = NbtList(NbtType.INT, listOf(NbtInt(2), NbtInt(1), NbtInt(1)))
        root["palette"] =
            NbtList(
                NbtType.COMPOUND,
                listOf(
                    NbtCompound().apply {
                        putString("Name", "minecraft:jigsaw")
                        this["Properties"] = NbtCompound().apply { putString("orientation", "north_up") }
                    },
                ),
            )
        root["blocks"] =
            NbtList(
                NbtType.COMPOUND,
                listOf(
                    blockEntry(0, 0).apply {
                        this["nbt"] =
                            NbtCompound().apply {
                                putString("id", "minecraft:jigsaw")
                                putString("final_state", "minecraft:chest[type=single,facing=east,waterlogged=false]")
                            }
                    },
                    blockEntry(1, 0).apply {
                        this["nbt"] =
                            NbtCompound().apply {
                                putString("id", "minecraft:jigsaw")
                                putString("final_state", "minecraft:structure_void")
                            }
                    },
                ),
            )
        val template = StructureTemplate().apply { load(root) }
        val chunk = Chunk(0, 0)
        chunk.setBlockStateId(4, 64, 4, Blocks.STONE.stateId)
        chunk.setBlockStateId(4, 64, 5, Blocks.STONE.stateId)

        template.placeInto(
            chunk,
            StructurePos(4, 64, 4),
            StructureRotation.CLOCKWISE_90,
            StructureMirror.NONE,
            StructurePos(0, 0, 0),
            StructureBoundingBox(4, 64, 4, 4, 64, 5),
        )

        assertEquals(3994, chunk.getBlockStateId(4, 64, 4))
        assertEquals(Blocks.STONE.stateId, chunk.getBlockStateId(4, 64, 5))
    }

    @Test
    fun loadsSizePaletteAndBlocks() {
        val template = StructureTemplate()
        template.load(syntheticTemplateNbt())

        assertEquals(StructurePos(2, 1, 1), template.size)
        assertEquals(2, template.blocks.size)
        val stoneId = Blocks.REGISTRY.byIdentifier(Identifier.vanilla("stone"))!!.stateId
        for (blockInfo in template.blocks) assertEquals(stoneId, blockInfo.stateId)
    }

    @Test
    fun placesBlocksAtTheUnrotatedWorldPosition() {
        val template = StructureTemplate()
        template.load(syntheticTemplateNbt())
        val chunk = Chunk(0, 0)

        val boundingBox =
            template.getBoundingBox(
                StructurePos(4, 64, 4),
                StructureRotation.NONE,
                StructurePos(0, 0, 0),
                StructureMirror.NONE,
            )
        template.placeInto(
            chunk,
            StructurePos(4, 64, 4),
            StructureRotation.NONE,
            StructureMirror.NONE,
            StructurePos(0, 0, 0),
            boundingBox,
        )

        val stoneId = Blocks.REGISTRY.byIdentifier(Identifier.vanilla("stone"))!!.stateId
        assertEquals(stoneId, chunk.getBlockStateId(4, 64, 4))
        assertEquals(stoneId, chunk.getBlockStateId(5, 64, 4))
        assertEquals(Blocks.AIR.stateId, chunk.getBlockStateId(6, 64, 4))
    }

    @Test
    fun rotating90DegreesSwapsTheAxisTheBlocksExtendAlong() {
        val template = StructureTemplate()
        template.load(syntheticTemplateNbt())
        val chunk = Chunk(0, 0)

        val boundingBox =
            template.getBoundingBox(
                StructurePos(4, 64, 4),
                StructureRotation.CLOCKWISE_90,
                StructurePos(0, 0, 0),
                StructureMirror.NONE,
            )
        template.placeInto(
            chunk,
            StructurePos(4, 64, 4),
            StructureRotation.CLOCKWISE_90,
            StructureMirror.NONE,
            StructurePos(0, 0, 0),
            boundingBox,
        )

        val stoneId = Blocks.REGISTRY.byIdentifier(Identifier.vanilla("stone"))!!.stateId
        assertEquals(stoneId, chunk.getBlockStateId(4, 64, 4))
        assertEquals(stoneId, chunk.getBlockStateId(4, 64, 5))
        assertEquals(Blocks.AIR.stateId, chunk.getBlockStateId(5, 64, 4))
    }

    @Test
    fun clippingToABoundingBoxDropsBlocksOutsideIt() {
        val template = StructureTemplate()
        template.load(syntheticTemplateNbt())
        val chunk = Chunk(0, 0)

        val narrowClip = StructureBoundingBox(4, 64, 4, 4, 64, 4)
        template.placeInto(
            chunk,
            StructurePos(4, 64, 4),
            StructureRotation.NONE,
            StructureMirror.NONE,
            StructurePos(0, 0, 0),
            narrowClip,
        )

        val stoneId = Blocks.REGISTRY.byIdentifier(Identifier.vanilla("stone"))!!.stateId
        assertEquals(stoneId, chunk.getBlockStateId(4, 64, 4))
        assertEquals(Blocks.AIR.stateId, chunk.getBlockStateId(5, 64, 4))
    }

    @Test
    fun structureAndAirIgnoreProcessorPreservesWorldBlocks() {
        val template = StructureTemplate()
        val root = NbtCompound()
        root["size"] = NbtList(NbtType.INT, listOf(NbtInt(2), NbtInt(1), NbtInt(1)))
        root["palette"] =
            NbtList(
                NbtType.COMPOUND,
                listOf(
                    NbtCompound().apply { putString("Name", "minecraft:air") },
                    NbtCompound().apply { putString("Name", "minecraft:structure_block") },
                ),
            )
        root["blocks"] =
            NbtList(
                NbtType.COMPOUND,
                listOf(
                    blockEntry(0, 0),
                    blockEntry(1, 1),
                ),
            )
        template.load(root)
        val chunk = Chunk(0, 0)
        chunk.setBlockStateId(4, 64, 4, Blocks.STONE.stateId)
        chunk.setBlockStateId(5, 64, 4, Blocks.STONE.stateId)
        template.placeInto(
            chunk,
            StructurePos(4, 64, 4),
            StructureRotation.NONE,
            StructureMirror.NONE,
            StructurePos(0, 0, 0),
            StructureBoundingBox(4, 64, 4, 5, 64, 4),
            ignoreStructureBlocks = true,
            ignoreAir = true,
        )

        assertEquals(Blocks.STONE.stateId, chunk.getBlockStateId(4, 64, 4))
        assertEquals(Blocks.STONE.stateId, chunk.getBlockStateId(5, 64, 4))
    }

    @Test
    fun ruleProcessorsUsePositionalRandomWithoutConsumingPlacementRandom() {
        val template = StructureTemplate().apply { load(syntheticTemplateNbt()) }
        val chunk = Chunk(0, 0)
        val placementRandom = WorldgenRandom(XoroshiroRandom(91L))
        val untouchedRandom = WorldgenRandom(XoroshiroRandom(91L))
        val andesiteId = Blocks.REGISTRY.byIdentifier(Identifier.vanilla("andesite"))!!.stateId
        val processors =
            StructureProcessorList(
                listOf(
                    StructureProcessorRule(
                        StructureRuleTest.RandomBlockMatch(Blocks.STONE.stateId, 1.0f),
                        null,
                        andesiteId,
                    ),
                ),
            )

        template.placeInto(
            chunk,
            StructurePos(4, 64, 4),
            StructureRotation.NONE,
            StructureMirror.NONE,
            StructurePos(0, 0, 0),
            StructureBoundingBox(4, 64, 4, 5, 64, 4),
            processors,
            placementRandom,
        )

        assertEquals(andesiteId, chunk.getBlockStateId(4, 64, 4))
        assertEquals(andesiteId, chunk.getBlockStateId(5, 64, 4))
        assertEquals(untouchedRandom.nextLong(), placementRandom.nextLong())
    }

    private fun blockEntry(
        x: Int,
        state: Int,
    ): NbtCompound =
        NbtCompound().apply {
            set("pos", NbtList(NbtType.INT, listOf(NbtInt(x), NbtInt(0), NbtInt(0))))
            putInt("state", state)
        }
}
