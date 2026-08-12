package org.basaltmc.server.world.terrain

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.ChunkHeight
import org.basaltmc.server.world.densityfunction.DensityFunctions
import org.basaltmc.server.world.structure.StructureMirror
import org.basaltmc.server.world.structure.StructurePlacementGrid
import org.basaltmc.server.world.structure.StructurePos
import org.basaltmc.server.world.structure.StructureTemplate
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtInt
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtType
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertTrue

class EndCityStructureGeneratorTest {
    @Test
    fun requiresOuterIslandBiomeAndMinimumSurfaceHeight() {
        val router = EndNoiseRouter(DensityFunctions.constant(1.0), DensityFunctions.constant(1.0))
        val biomes = TheEndBiomeSource(router.erosion)

        val high = EndCityStructureGenerator(router, biomes, TEMPLATE_LOADER) { _, _ -> 70 }
        assertTrue(high.cityAt(0L, 80, 0).isNotEmpty())
        assertTrue(high.cityAt(0L, 3, 4).isEmpty(), "central island is the_end, not an End City biome")

        val low = EndCityStructureGenerator(router, biomes, TEMPLATE_LOADER) { _, _ -> 59 }
        assertTrue(low.cityAt(0L, 80, 0).isEmpty())
    }

    @Test
    fun placesTemplatesChestLootAndMarkerEntities() {
        val seed = 0L
        val router = EndNoiseRouter(DensityFunctions.constant(1.0), DensityFunctions.constant(1.0))
        val biomes = TheEndBiomeSource(router.erosion)
        val generator = EndCityStructureGenerator(router, biomes, TEMPLATE_LOADER) { _, _ -> 70 }
        val grid =
            StructurePlacementGrid(
                spacing = 20,
                separation = 11,
                salt = 10387313,
                spreadType = StructurePlacementGrid.SpreadType.TRIANGULAR,
            )
        val candidate = grid.candidateChunk(seed, 80, 0)
        val chunk = Chunk(candidate.first, candidate.second, ChunkHeight.END)
        val firstPiece = generator.cityAt(seed, candidate.first, candidate.second).first()
        val markers =
            firstPiece.template.dataMarkers(
                firstPiece.position,
                firstPiece.rotation,
                StructureMirror.NONE,
                StructurePos(0, 0, 0),
            )

        generator.generateInto(chunk, seed)

        assertEquals(Blocks.END_STONE.stateId, chunk.getBlockStateId(7, 70, 7))
        val chestMarker = markers.single { it.metadata == "Chest" }.pos
        val chest = assertNotNull(chunk.blockEntityAt(chestMarker.x and 15, chestMarker.y - 1, chestMarker.z and 15))
        assertEquals("minecraft:chests/end_city_treasure", chest.data.getString("LootTable"))
        val oracle = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = oracle.setDecorationSeed(seed, candidate.first * 16, candidate.second * 16)
        oracle.setFeatureSeed(decorationSeed, 2, 4)
        oracle.nextLong()
        assertEquals(oracle.nextLong(), chest.data.getLong("LootTableSeed"))

        val sentry = markers.single { it.metadata == "Sentry" }.pos
        assertTrue(
            chunk.generatedEntities().any {
                it.type == Identifier.vanilla("shulker") &&
                    it.x == sentry.x + 0.5 &&
                    it.y == sentry.y.toDouble() &&
                    it.z == sentry.z + 0.5
            },
        )
        assertTrue(
            chunk.generatedEntities().any {
                it.type == Identifier.vanilla("item_frame") &&
                    it.data.getString("item") == "minecraft:elytra"
            },
        )
    }

    private companion object {
        val TEMPLATE_SIZES =
            mapOf(
                "base_floor" to StructurePos(10, 4, 10),
                "base_roof" to StructurePos(12, 2, 12),
                "second_floor_1" to StructurePos(12, 8, 12),
                "second_floor_2" to StructurePos(12, 8, 12),
                "second_roof" to StructurePos(14, 2, 14),
                "third_floor_1" to StructurePos(14, 8, 14),
                "third_floor_2" to StructurePos(14, 8, 14),
                "third_roof" to StructurePos(16, 2, 16),
                "tower_base" to StructurePos(7, 7, 7),
                "tower_piece" to StructurePos(7, 4, 7),
                "tower_top" to StructurePos(9, 5, 9),
                "bridge_end" to StructurePos(5, 6, 2),
                "bridge_piece" to StructurePos(5, 6, 4),
                "bridge_steep_stairs" to StructurePos(5, 7, 4),
                "bridge_gentle_stairs" to StructurePos(5, 7, 8),
                "fat_tower_base" to StructurePos(13, 4, 13),
                "fat_tower_middle" to StructurePos(13, 8, 13),
                "fat_tower_top" to StructurePos(17, 6, 17),
                "ship" to StructurePos(13, 24, 29),
            )

        val TEMPLATE_LOADER: (String) -> StructureTemplate? = { location ->
            val name = location.substringAfterLast('/')
            TEMPLATE_SIZES[name]?.let { size ->
                if (name ==
                    "base_floor"
                ) {
                    markerTemplate(size)
                } else {
                    emptyTemplate(size)
                }
            }
        }

        fun emptyTemplate(size: StructurePos): StructureTemplate =
            StructureTemplate().apply { load(templateRoot(size, emptyList(), emptyList())) }

        fun markerTemplate(size: StructurePos): StructureTemplate {
            val palette =
                listOf(
                    palette("minecraft:end_stone"),
                    palette("minecraft:chest"),
                    palette("minecraft:structure_block"),
                )
            val blocks =
                listOf(
                    block(0, 0, 0, 0),
                    block(1, 0, 1, 1, NbtCompound().apply { putString("id", "minecraft:chest") }),
                    marker(1, 1, 1, "Chest"),
                    marker(2, 1, 2, "Sentry"),
                    marker(3, 1, 3, "Elytra"),
                )
            return StructureTemplate().apply { load(templateRoot(size, palette, blocks)) }
        }

        fun templateRoot(
            size: StructurePos,
            palette: List<NbtCompound>,
            blocks: List<NbtCompound>,
        ): NbtCompound =
            NbtCompound().apply {
                this["size"] = NbtList(NbtType.INT, listOf(NbtInt(size.x), NbtInt(size.y), NbtInt(size.z)))
                this["palette"] = NbtList(NbtType.COMPOUND, palette)
                this["blocks"] = NbtList(NbtType.COMPOUND, blocks)
            }

        fun palette(name: String): NbtCompound = NbtCompound().apply { putString("Name", name) }

        fun marker(
            x: Int,
            y: Int,
            z: Int,
            metadata: String,
        ): NbtCompound =
            block(
                x,
                y,
                z,
                2,
                NbtCompound().apply {
                    putString("id", "minecraft:structure_block")
                    putString("mode", "DATA")
                    putString("metadata", metadata)
                },
            )

        fun block(
            x: Int,
            y: Int,
            z: Int,
            state: Int,
            nbt: NbtCompound? = null,
        ): NbtCompound =
            NbtCompound().apply {
                this["pos"] = NbtList(NbtType.INT, listOf(NbtInt(x), NbtInt(y), NbtInt(z)))
                putInt("state", state)
                if (nbt != null) this["nbt"] = nbt
            }
    }
}
