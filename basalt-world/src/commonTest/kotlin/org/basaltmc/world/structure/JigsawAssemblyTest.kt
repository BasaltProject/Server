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
                    putString("final_state", "minecraft:air")
                },
            )
        }
    root["blocks"] = NbtList(NbtType.COMPOUND, listOf(stoneBlock, jigsawBlock))
    return root
}

class JigsawAssemblyTest {
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
