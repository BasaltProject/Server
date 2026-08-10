package org.basaltmc.world.terrain

import org.basaltmc.nbt.NbtCompound
import org.basaltmc.nbt.NbtInt
import org.basaltmc.nbt.NbtList
import org.basaltmc.nbt.NbtType
import org.basaltmc.registry.Blocks
import org.basaltmc.world.Chunk
import org.basaltmc.world.ChunkHeight
import org.basaltmc.world.densityfunction.DensityFunction
import org.basaltmc.world.densityfunction.FunctionContext
import org.basaltmc.world.structure.StructureBoundingBox
import org.basaltmc.world.structure.StructureMirror
import org.basaltmc.world.structure.StructurePlacementGrid
import org.basaltmc.world.structure.StructurePos
import org.basaltmc.world.structure.StructureRotation
import org.basaltmc.world.structure.StructureTemplate
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertTrue

class NetherRuinedPortalGeneratorTest {
    @Test
    fun startsMatchOfficialLegacyRngVectors() {
        val template = portalTemplate()
        val generator = generator(template)
        val grid = StructurePlacementGrid(spacing = 40, separation = 15, salt = 34222645)
        val cases =
            listOf(
                ExpectedStart(
                    0L,
                    false,
                    "minecraft:ruined_portal/portal_8",
                    StructureRotation.CLOCKWISE_180,
                    StructureMirror.FRONT_BACK,
                    StructurePos(320, 27, 144),
                    StructureBoundingBox(324, 27, 144, 328, 31, 148),
                ),
                ExpectedStart(
                    1L,
                    false,
                    "minecraft:ruined_portal/portal_1",
                    StructureRotation.CLOCKWISE_90,
                    StructureMirror.NONE,
                    StructurePos(304, 64, 288),
                    StructureBoundingBox(304, 64, 288, 308, 68, 292),
                ),
                ExpectedStart(
                    2L,
                    true,
                    "minecraft:ruined_portal/portal_3",
                    StructureRotation.CLOCKWISE_90,
                    StructureMirror.FRONT_BACK,
                    StructurePos(304, 64, 96),
                    StructureBoundingBox(304, 64, 92, 308, 68, 96),
                ),
                ExpectedStart(
                    42L,
                    false,
                    "minecraft:ruined_portal/portal_2",
                    StructureRotation.CLOCKWISE_180,
                    StructureMirror.FRONT_BACK,
                    StructurePos(256, 29, 352),
                    StructureBoundingBox(260, 29, 352, 264, 33, 356),
                ),
            )

        for (expected in cases) {
            val candidate = grid.candidateChunk(expected.seed, 0, 0)
            val actual = assertNotNull(generator.startSnapshot(expected.seed, candidate.first, candidate.second))
            assertEquals(expected.airPocket, actual.airPocket, "seed ${expected.seed} air pocket")
            assertEquals(expected.templateName, actual.templateName, "seed ${expected.seed} template")
            assertEquals(expected.rotation, actual.rotation, "seed ${expected.seed} rotation")
            assertEquals(expected.mirror, actual.mirror, "seed ${expected.seed} mirror")
            assertEquals(expected.position, actual.position, "seed ${expected.seed} position")
            assertEquals(expected.boundingBox, actual.boundingBox, "seed ${expected.seed} box")
        }
    }

    @Test
    fun templateProcessorsChestSeedAndCrossChunkSpreadMatchTheCenterReplay() {
        val seed = 2L
        val template = portalTemplate()
        val generator = generator(template)
        val grid = StructurePlacementGrid(spacing = 40, separation = 15, salt = 34222645)
        val candidate = grid.candidateChunk(seed, 0, 0)
        val start = assertNotNull(generator.startSnapshot(seed, candidate.first, candidate.second))
        val centerX = start.boundingBox.minX + (start.boundingBox.maxX - start.boundingBox.minX + 1) / 2
        val centerZ = start.boundingBox.minZ + (start.boundingBox.maxZ - start.boundingBox.minZ + 1) / 2
        val chunks = mutableListOf<Chunk>()
        for (chunkX in (centerX - 14).floorDiv(16)..(centerX + 14).floorDiv(16)) {
            for (chunkZ in (centerZ - 14).floorDiv(16)..(centerZ + 14).floorDiv(16)) {
                chunks += Chunk(chunkX, chunkZ, ChunkHeight.NETHER).also { generator.generateInto(it, seed) }
            }
        }

        val entities = chunks.flatMap { it.blockEntities() }
        assertEquals(1, entities.size)
        assertEquals("minecraft:chests/ruined_portal", entities.single().data.getString("LootTable"))
        assertEquals(2202739359229700781L, entities.single().data.getLong("LootTableSeed"))
        assertEquals(
            0,
            entities
                .single()
                .data
                .getCompound("components")
                .size,
        )
        val chestState =
            chunks
                .asSequence()
                .flatMap(::states)
                .mapNotNull(Blocks::byStateId)
                .single { it.block.path == "chest" }
        assertEquals("north", chestState.properties["facing"])

        val paths =
            chunks
                .asSequence()
                .flatMap(::states)
                .mapNotNull(Blocks::byStateId)
                .map { it.block.path }
                .toList()
        assertTrue(paths.any { it.startsWith("polished_blackstone") || it.contains("polished_blackstone") })
        assertTrue(paths.count { it == "netherrack" || it == "magma_block" } > 100)
        val centerChunk = centerX.floorDiv(16) to centerZ.floorDiv(16)
        assertTrue(
            chunks
                .filter { it.chunkX to it.chunkZ != centerChunk }
                .any { chunk -> states(chunk).mapNotNull(Blocks::byStateId).any { it.block.path == "netherrack" } },
        )
    }

    private fun generator(template: StructureTemplate): NetherRuinedPortalGenerator =
        NetherRuinedPortalGenerator(
            NetherBiomeSource(ConstantDensity(0.0), ConstantDensity(0.0)),
            templateLoader = { if (it.startsWith("minecraft:ruined_portal/")) template else null },
            baseStateAt = { _, y, _ -> if (y <= 64) Blocks.NETHERRACK.stateId else Blocks.AIR.stateId },
            placeEarlierBastions = { _, _ -> },
        )

    private fun portalTemplate(): StructureTemplate {
        val root = NbtCompound()
        root["size"] = intList(5, 5, 5)
        root["palette"] =
            NbtList(
                NbtType.COMPOUND,
                listOf(
                    palette("netherrack"),
                    palette("stone_bricks"),
                    palette("obsidian"),
                    palette("gold_block"),
                    palette("air"),
                    palette(
                        "chest",
                        linkedMapOf("type" to "single", "facing" to "east", "waterlogged" to "false"),
                    ),
                    palette("jigsaw", linkedMapOf("orientation" to "north_up")),
                ),
            )
        root["blocks"] =
            NbtList(
                NbtType.COMPOUND,
                listOf(
                    block(2, 0, 2, 0),
                    block(2, 1, 2, 1),
                    block(2, 2, 2, 2),
                    block(3, 1, 2, 3),
                    block(0, 1, 0, 4),
                    block(
                        1,
                        1,
                        2,
                        5,
                        NbtCompound().apply {
                            putString("id", "minecraft:chest")
                            putString("LootTable", "minecraft:chests/ruined_portal")
                        },
                    ),
                    block(
                        2,
                        0,
                        1,
                        6,
                        NbtCompound().apply {
                            putString("id", "minecraft:jigsaw")
                            putString("final_state", "minecraft:netherrack")
                        },
                    ),
                ),
            )
        return StructureTemplate().apply { load(root) }
    }

    private fun palette(
        path: String,
        properties: Map<String, String> = emptyMap(),
    ): NbtCompound =
        NbtCompound().apply {
            putString("Name", "minecraft:$path")
            if (properties.isNotEmpty()) {
                this["Properties"] = NbtCompound().apply { for ((name, value) in properties) putString(name, value) }
            }
        }

    private fun block(
        x: Int,
        y: Int,
        z: Int,
        state: Int,
        nbt: NbtCompound? = null,
    ): NbtCompound =
        NbtCompound().apply {
            this["pos"] = intList(x, y, z)
            putInt("state", state)
            if (nbt != null) this["nbt"] = nbt
        }

    private fun intList(vararg values: Int): NbtList = NbtList(NbtType.INT, values.map(::NbtInt))

    private fun states(chunk: Chunk): Sequence<Int> =
        sequence {
            for (y in 0..127) {
                for (z in 0 until 16) {
                    for (x in 0 until 16) yield(chunk.getBlockStateId(x, y, z))
                }
            }
        }

    private data class ExpectedStart(
        val seed: Long,
        val airPocket: Boolean,
        val templateName: String,
        val rotation: StructureRotation,
        val mirror: StructureMirror,
        val position: StructurePos,
        val boundingBox: StructureBoundingBox,
    )

    private class ConstantDensity(
        private val value: Double,
    ) : DensityFunction {
        override fun compute(context: FunctionContext): Double = value

        override fun minValue(): Double = value

        override fun maxValue(): Double = value
    }
}
