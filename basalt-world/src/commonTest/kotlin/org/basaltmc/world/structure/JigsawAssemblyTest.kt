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
import kotlin.test.assertTrue

/**
 * A 2x2x2 stone box with a single EAST-facing jigsaw block on one face, targeting [targetName] in
 * [poolName], synthetic NBT built by hand (not extracted from any Mojang file).
 */
private fun jigsawBoxNbt(
    poolName: String,
    ownName: String,
    targetName: String,
    finalState: String = "minecraft:air",
): NbtCompound {
    val root = NbtCompound()
    root["size"] = NbtList(NbtType.INT, listOf(NbtInt(2), NbtInt(1), NbtInt(1)))

    val stoneEntry = NbtCompound().apply { putString("Name", "minecraft:stone") }
    val jigsawEntry =
        NbtCompound().apply {
            set("Properties", NbtCompound().apply { putString("orientation", "east_up") })
            putString("Name", "minecraft:jigsaw")
        }
    root["palette"] = NbtList(NbtType.COMPOUND, listOf(stoneEntry, jigsawEntry))

    val stoneBlock =
        NbtCompound().apply {
            set("pos", NbtList(NbtType.INT, listOf(NbtInt(0), NbtInt(0), NbtInt(0))))
            putInt("state", 0)
        }
    val jigsawBlock =
        NbtCompound().apply {
            set("pos", NbtList(NbtType.INT, listOf(NbtInt(1), NbtInt(0), NbtInt(0))))
            putInt("state", 1)
            set(
                "nbt",
                NbtCompound().apply {
                    putString("name", ownName)
                    putString("target", targetName)
                    putString("pool", poolName)
                    putString("final_state", finalState)
                },
            )
        }
    root["blocks"] = NbtList(NbtType.COMPOUND, listOf(stoneBlock, jigsawBlock))
    return root
}

private fun singleJigsawNbt(
    sizeX: Int,
    jigsawX: Int,
    orientation: String,
    poolName: String,
    ownName: String,
    targetName: String,
): NbtCompound {
    val root = NbtCompound()
    root["size"] = NbtList(NbtType.INT, listOf(NbtInt(sizeX), NbtInt(1), NbtInt(1)))
    root["palette"] =
        NbtList(
            NbtType.COMPOUND,
            listOf(
                NbtCompound().apply {
                    putString("Name", "minecraft:jigsaw")
                    set("Properties", NbtCompound().apply { putString("orientation", orientation) })
                },
            ),
        )
    root["blocks"] =
        NbtList(
            NbtType.COMPOUND,
            listOf(
                NbtCompound().apply {
                    set("pos", NbtList(NbtType.INT, listOf(NbtInt(jigsawX), NbtInt(0), NbtInt(0))))
                    putInt("state", 0)
                    set(
                        "nbt",
                        NbtCompound().apply {
                            putString("name", ownName)
                            putString("target", targetName)
                            putString("pool", poolName)
                            putString("final_state", "minecraft:air")
                        },
                    )
                },
            ),
        )
    return root
}

private fun twoJigsawNbt(
    sizeX: Int,
    firstX: Int,
    firstOrientation: String,
    firstPool: String,
    firstName: String,
    firstTarget: String,
    secondX: Int,
    secondOrientation: String,
    secondPool: String,
    secondName: String,
    secondTarget: String,
): NbtCompound {
    val root = NbtCompound()
    root["size"] = NbtList(NbtType.INT, listOf(NbtInt(sizeX), NbtInt(1), NbtInt(1)))
    root["palette"] =
        NbtList(
            NbtType.COMPOUND,
            listOf(firstOrientation, secondOrientation).map { orientation ->
                NbtCompound().apply {
                    putString("Name", "minecraft:jigsaw")
                    set("Properties", NbtCompound().apply { putString("orientation", orientation) })
                }
            },
        )

    fun block(
        x: Int,
        state: Int,
        pool: String,
        name: String,
        target: String,
    ): NbtCompound =
        NbtCompound().apply {
            set("pos", NbtList(NbtType.INT, listOf(NbtInt(x), NbtInt(0), NbtInt(0))))
            putInt("state", state)
            set(
                "nbt",
                NbtCompound().apply {
                    putString("name", name)
                    putString("target", target)
                    putString("pool", pool)
                    putString("final_state", "minecraft:air")
                },
            )
        }
    root["blocks"] =
        NbtList(
            NbtType.COMPOUND,
            listOf(
                block(firstX, 0, firstPool, firstName, firstTarget),
                block(secondX, 1, secondPool, secondName, secondTarget),
            ),
        )
    return root
}

class JigsawAssemblyTest {
    @Test
    fun recordsVanillaGroundDeltasAndBidirectionalJunctions() {
        val source =
            StructureTemplate().apply {
                load(singleJigsawNbt(2, 1, "east_up", "test:next", "minecraft:start", "minecraft:next"))
            }
        val target =
            StructureTemplate().apply {
                load(singleJigsawNbt(1, 0, "west_up", "minecraft:empty", "minecraft:next", "minecraft:none"))
            }
        val templates = mapOf("test:source" to source, "test:target" to target)
        val pools =
            mapOf(
                "test:start" to
                    StructureTemplatePool(
                        "minecraft:empty",
                        listOf(
                            LegacySinglePoolElement(
                                "test:source",
                                StructureProcessorList.EMPTY,
                                StructureProjection.RIGID,
                            ) to 1,
                        ),
                    ),
                "test:next" to
                    StructureTemplatePool(
                        "minecraft:empty",
                        listOf(
                            LegacySinglePoolElement(
                                "test:target",
                                StructureProcessorList.EMPTY,
                                StructureProjection.RIGID,
                            ) to 1,
                        ),
                    ),
                "minecraft:empty" to StructureTemplatePool("minecraft:empty", emptyList()),
            )
        val pieces =
            JigsawAssembly({ pools[it] }, { templates[it] }, heightAt = { _, _ -> 64 }).assemble(
                "test:start",
                StructurePos(0, 64, 0),
                maxDepth = 1,
                WorldgenRandom(XoroshiroRandom(2L)),
                projectStartToHeightmap = false,
                maxDistanceFromCenter = 80,
            )

        assertEquals(2, pieces.size)
        assertEquals(1, pieces[0].groundLevelDelta)
        assertEquals(1, pieces[1].groundLevelDelta)
        val sourceJunction = pieces[0].junctions.single()
        val targetJunction = pieces[1].junctions.single()
        assertEquals(-sourceJunction.deltaY, targetJunction.deltaY)
        assertEquals(StructureProjection.RIGID, sourceJunction.destinationProjection)
        assertEquals(StructureProjection.RIGID, targetJunction.destinationProjection)
        assertEquals(sourceJunction.sourceGroundY, targetJunction.sourceGroundY)
        assertEquals(64, sourceJunction.sourceGroundY)
    }

    @Test
    fun villageExpansionHackUsesTallestChildPoolHeight() {
        val source =
            StructureTemplate().apply {
                load(singleJigsawNbt(2, 1, "east_up", "test:next", "minecraft:start", "minecraft:next"))
            }
        val candidate =
            StructureTemplate().apply {
                load(
                    twoJigsawNbt(
                        3,
                        0,
                        "west_up",
                        "minecraft:empty",
                        "minecraft:next",
                        "minecraft:none",
                        1,
                        "east_up",
                        "test:tall",
                        "minecraft:child",
                        "minecraft:tall",
                    ),
                )
            }
        val tall =
            StructureTemplate().apply {
                load(
                    singleJigsawNbt(1, 0, "west_up", "minecraft:empty", "minecraft:tall", "minecraft:none").also {
                        it["size"] =
                            NbtList(NbtType.INT, listOf(NbtInt(1), NbtInt(20), NbtInt(1)))
                    },
                )
            }
        val templates = mapOf("test:source" to source, "test:candidate" to candidate, "test:tall" to tall)

        fun pool(location: String) =
            StructureTemplatePool(
                "minecraft:empty",
                listOf(
                    LegacySinglePoolElement(location, StructureProcessorList.EMPTY, StructureProjection.RIGID) to 1,
                ),
            )
        val pools =
            mapOf(
                "test:start" to pool("test:source"),
                "test:next" to pool("test:candidate"),
                "test:tall" to pool("test:tall"),
                "minecraft:empty" to StructureTemplatePool("minecraft:empty", emptyList()),
            )
        val assembly = JigsawAssembly({ pools[it] }, { templates[it] }, heightAt = { _, _ -> 64 })

        fun assemble(useHack: Boolean) =
            assembly.assemble(
                "test:start",
                StructurePos(0, 64, 0),
                maxDepth = 1,
                WorldgenRandom(XoroshiroRandom(4L)),
                projectStartToHeightmap = false,
                maxDistanceFromCenter = 80,
                useExpansionHack = useHack,
            )

        val ordinary = assemble(false)[1].boundingBox
        val expanded = assemble(true)[1].boundingBox
        assertEquals(1, ordinary.maxY - ordinary.minY + 1)
        assertEquals(22, expanded.maxY - expanded.minY + 1)
    }

    @Test
    fun inwardFacingJigsawCanPlaceAChildInsideItsSourcePiece() {
        val source =
            StructureTemplate().apply {
                load(singleJigsawNbt(3, 2, "west_up", "test:inside", "minecraft:start", "minecraft:inside"))
            }
        val inside =
            StructureTemplate().apply {
                load(singleJigsawNbt(1, 0, "east_up", "minecraft:empty", "minecraft:inside", "minecraft:unused"))
            }
        val templates = mapOf("test:source" to source, "test:inside" to inside)
        val sourcePool =
            StructureTemplatePool(
                "minecraft:empty",
                listOf(
                    LegacySinglePoolElement(
                        "test:source",
                        StructureProcessorList.EMPTY,
                        StructureProjection.RIGID,
                    ) to 1,
                ),
            )
        val insidePool =
            StructureTemplatePool(
                "minecraft:empty",
                listOf(
                    LegacySinglePoolElement(
                        "test:inside",
                        StructureProcessorList.EMPTY,
                        StructureProjection.RIGID,
                    ) to 1,
                ),
            )
        val pools =
            mapOf(
                "test:start" to sourcePool,
                "test:inside" to insidePool,
                "minecraft:empty" to StructureTemplatePool("minecraft:empty", emptyList()),
            )
        val pieces =
            JigsawAssembly({ pools[it] }, { templates[it] }, heightAt = { _, _ -> 64 }).assemble(
                "test:start",
                StructurePos(0, 64, 0),
                maxDepth = 1,
                WorldgenRandom(XoroshiroRandom(12L)),
                projectStartToHeightmap = false,
                maxDistanceFromCenter = 80,
            )

        assertEquals(2, pieces.size)
        assertTrue(pieces.first().boundingBox.containsFully(pieces.last().boundingBox))
    }

    @Test
    fun jigsawFinalStateAcceptsBracketedProperties() {
        val template =
            StructureTemplate().apply {
                load(jigsawBoxNbt("test:next", "minecraft:start", "minecraft:bottom", "minecraft:stone[foo=bar]"))
            }
        val chunk = Chunk(0, 0)
        template.placeInto(
            chunk,
            StructurePos(4, 64, 4),
            StructureRotation.NONE,
            StructureMirror.NONE,
            StructurePos(0, 0, 0),
            StructureBoundingBox(4, 64, 4, 5, 64, 4),
        )

        assertEquals(Blocks.STONE.stateId, chunk.getBlockStateId(5, 64, 4))
    }

    @Test
    fun readsAlignedJointTopFacingAndPriorities() {
        val template =
            StructureTemplate().apply {
                load(
                    jigsawBoxNbt("test:next", "minecraft:start", "minecraft:bottom"),
                )
            }
        val jigsaw =
            template
                .jigsawBlocks(
                    StructurePos(0, 0, 0),
                    StructureRotation.NONE,
                    StructureMirror.NONE,
                    StructurePos(0, 0, 0),
                ).single()

        assertEquals(StructureDirection.EAST, jigsaw.front)
        assertEquals(StructureDirection.UP, jigsaw.top)
        assertEquals(false, jigsaw.rollable)
        assertEquals(0, jigsaw.placementPriority)
        assertEquals(0, jigsaw.selectionPriority)
    }

    @Test
    fun connectsTwoPiecesThroughAMatchingJigsawPair() {
        val startTemplate =
            StructureTemplate().apply {
                load(
                    jigsawBoxNbt("test:next", "minecraft:start", "minecraft:bottom"),
                )
            }
        val nextTemplate =
            StructureTemplate().apply {
                load(jigsawBoxNbt("test:unused", "minecraft:bottom", "minecraft:unused"))
            }

        val templates = mapOf("test:start_piece" to startTemplate, "test:next_piece" to nextTemplate)
        val loader: TemplateLoader = { templates[it] }

        val startPool =
            StructureTemplatePool(
                "minecraft:empty",
                listOf(
                    LegacySinglePoolElement(
                        "test:start_piece",
                        StructureProcessorList.EMPTY,
                        StructureProjection.RIGID,
                    ) to
                        1,
                ),
            )
        val nextPool =
            StructureTemplatePool(
                "minecraft:empty",
                listOf(
                    LegacySinglePoolElement(
                        "test:next_piece",
                        StructureProcessorList.EMPTY,
                        StructureProjection.RIGID,
                    ) to
                        1,
                ),
            )
        val emptyPool = StructureTemplatePool("minecraft:empty", emptyList())
        val pools = mapOf("test:start" to startPool, "test:next" to nextPool, "minecraft:empty" to emptyPool)

        val assembly = JigsawAssembly(poolLookup = { pools[it] }, templateLoader = loader, heightAt = { _, _ -> 64 })
        val pieces =
            assembly.assemble(
                "test:start",
                StructurePos(0, 64, 0),
                maxDepth = 3,
                WorldgenRandom(XoroshiroRandom(1L)),
                projectStartToHeightmap = false,
                maxDistanceFromCenter = 80,
            )

        assertEquals(2, pieces.size)
        assertTrue(!pieces[0].boundingBox.intersects(pieces[1].boundingBox), "connected pieces must not overlap")
    }

    @Test
    fun placingAssembledPiecesWritesRealBlocksIntoTheChunk() {
        val startTemplate =
            StructureTemplate().apply {
                load(
                    jigsawBoxNbt("test:next", "minecraft:start", "minecraft:bottom"),
                )
            }
        val nextTemplate =
            StructureTemplate().apply {
                load(jigsawBoxNbt("test:unused", "minecraft:bottom", "minecraft:unused"))
            }
        val templates = mapOf("test:start_piece" to startTemplate, "test:next_piece" to nextTemplate)
        val loader: TemplateLoader = { templates[it] }

        val startPool =
            StructureTemplatePool(
                "minecraft:empty",
                listOf(
                    LegacySinglePoolElement(
                        "test:start_piece",
                        StructureProcessorList.EMPTY,
                        StructureProjection.RIGID,
                    ) to
                        1,
                ),
            )
        val nextPool =
            StructureTemplatePool(
                "minecraft:empty",
                listOf(
                    LegacySinglePoolElement(
                        "test:next_piece",
                        StructureProcessorList.EMPTY,
                        StructureProjection.RIGID,
                    ) to
                        1,
                ),
            )
        val emptyPool = StructureTemplatePool("minecraft:empty", emptyList())
        val pools = mapOf("test:start" to startPool, "test:next" to nextPool, "minecraft:empty" to emptyPool)

        val assembly = JigsawAssembly(poolLookup = { pools[it] }, templateLoader = loader, heightAt = { _, _ -> 64 })
        val pieces =
            assembly.assemble(
                "test:start",
                StructurePos(4, 64, 4),
                maxDepth = 3,
                WorldgenRandom(XoroshiroRandom(7L)),
                projectStartToHeightmap = false,
                maxDistanceFromCenter = 80,
            )

        val chunk = Chunk(0, 0)
        val random = WorldgenRandom(XoroshiroRandom(7L))
        for (piece in pieces) {
            val clip =
                StructureBoundingBox(
                    0,
                    org.basaltmc.world.WorldHeight.MIN_Y,
                    0,
                    15,
                    org.basaltmc.world.WorldHeight.MAX_Y,
                    15,
                )
            piece.element.place(loader, chunk, piece.position, piece.rotation, clip, random)
        }

        val stoneId = Blocks.REGISTRY.byIdentifier(Identifier.vanilla("stone"))!!.stateId
        var stoneCount = 0
        for (y in org.basaltmc.world.WorldHeight.MIN_Y..org.basaltmc.world.WorldHeight.MAX_Y) {
            for (x in 0 until 16) for (z in 0 until 16) if (chunk.getBlockStateId(x, y, z) == stoneId) stoneCount++
        }
        assertEquals(pieces.size, stoneCount, "each placed piece should have placed exactly its one stone block")
    }

    /**
     * Regression test for a real bug: a pool that keeps connecting to copies of itself (every
     * real chain-like pool, like village streets connecting to more streets) can extend forever
     * unless something outer-bounds the search, exactly like real vanilla's own `maxDistanceFromCenter`
     * does. A stress test hammering real Plains village generation across a wide area found this
     * missing bound produced assemblies that never stopped growing (`maxDepth` alone wasn't a
     * tight enough limit for real pool branching), pegging every worker thread's CPU. `maxDepth`
     * here is deliberately absurd (1000) so only [maxDistanceFromCenter] can be what stops it.
     */
    @Test
    fun boundsAssemblyToMaxDistanceFromCenterEvenWithASelfReferentialChainPool() {
        val chainTemplate =
            StructureTemplate().apply {
                load(jigsawBoxNbt("test:chain", "minecraft:chain_link", "minecraft:chain_link"))
            }
        val templates = mapOf("test:chain_piece" to chainTemplate)
        val loader: TemplateLoader = { templates[it] }
        val chainPool =
            StructureTemplatePool(
                "minecraft:empty",
                listOf(
                    LegacySinglePoolElement(
                        "test:chain_piece",
                        StructureProcessorList.EMPTY,
                        StructureProjection.RIGID,
                    ) to
                        1,
                ),
            )
        val emptyPool = StructureTemplatePool("minecraft:empty", emptyList())
        val pools = mapOf("test:chain" to chainPool, "minecraft:empty" to emptyPool)

        val assembly = JigsawAssembly(poolLookup = { pools[it] }, templateLoader = loader, heightAt = { _, _ -> 64 })
        val pieces =
            assembly.assemble(
                "test:chain",
                StructurePos(0, 64, 0),
                maxDepth = 1000,
                WorldgenRandom(XoroshiroRandom(1L)),
                projectStartToHeightmap = false,
                maxDistanceFromCenter = 20,
            )

        assertTrue(
            pieces.size in 1..25,
            "expected the chain to stop well within maxDistanceFromCenter, got ${pieces.size} pieces",
        )
        for (piece in pieces) {
            assertTrue(
                piece.boundingBox.minX >= -20 && piece.boundingBox.maxX <= 20,
                "piece at ${piece.boundingBox} escaped maxDistanceFromCenter",
            )
        }
    }
}
