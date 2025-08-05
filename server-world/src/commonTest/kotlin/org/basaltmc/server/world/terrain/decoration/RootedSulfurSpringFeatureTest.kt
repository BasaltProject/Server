package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.structure.StructurePos
import org.basaltmc.server.world.structure.StructureRotation
import org.basaltmc.server.world.structure.StructureTemplate
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtInt
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtType
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

class RootedSulfurSpringFeatureTest {
    @Test
    fun contextualFeatureBuildsTheRootSystemTuffPatchAndTemplate() {
        val chunk = ceilingChunk()
        val loadedTemplate = markerTemplate()
        var selectedTemplate: Identifier? = null
        val placement = PlacedFeatures.contextual(ROOTED_SULFUR_ID) ?: error("missing contextual feature")
        val random = random()

        assertTrue(
            placement.place(
                chunk,
                random,
                ORIGIN.x,
                ORIGIN.y,
                ORIGIN.z,
                PlacedFeatures.FeatureContext(
                    templates = { id ->
                        selectedTemplate = id
                        loadedTemplate
                    },
                    blockStateAt = blockLookup(chunk),
                ),
            ),
        )

        assertEquals(Identifier.vanilla("spring/sulfur_spring_small_4"), selectedTemplate)
        assertTrue(count(chunk, "tuff") > 0)
        assertTrue(count(chunk, "sulfur") > 0)
        assertEquals(1, count(chunk, "cinnabar"))
        assertEquals("cinnabar", path(chunk.getBlockStateId(10, 23, 7)))
        assertEquals("sulfur", path(chunk.getBlockStateId(ORIGIN.x, ORIGIN.y, ORIGIN.z)))
        // Official 26.2 RNG oracle: weighted variant/template, 64 tuff offsets, CW90 rotation,
        // single-palette draw, 180 root attempts and six hanging-root provider draws.
        assertEquals(127919215324369005L, random.nextLong())
    }

    @Test
    fun levelProbeRejectsAnUnevenCeilingBeforeTheNestedFeatureRuns() {
        val chunk = ceilingChunk()
        var loaded = false
        val baseLookup = blockLookup(chunk)
        val placement = PlacedFeatures.contextual(ROOTED_SULFUR_ID) ?: error("missing contextual feature")

        assertTrue(
            placement.place(
                chunk,
                random(),
                ORIGIN.x,
                ORIGIN.y,
                ORIGIN.z,
                PlacedFeatures.FeatureContext(
                    templates = {
                        loaded = true
                        markerTemplate()
                    },
                    blockStateAt = { x, y, z ->
                        if (x == ORIGIN.x + 8 && y == CEILING_TOP - 1 && z == ORIGIN.z) {
                            Blocks.AIR.stateId
                        } else {
                            baseLookup(x, y, z)
                        }
                    },
                ),
            ),
        )

        assertFalse(loaded)
        assertEquals(0, count(chunk, "tuff"))
        assertEquals(0, count(chunk, "sulfur"))
    }

    @Test
    fun templateDecorationWritesCrossChunkCoordinatesWithoutWrapping() {
        val template = threeBlockTemplate()
        val chunk = Chunk(0, 0)

        assertTrue(
            template.placeDecorationInto(
                chunk,
                StructurePos(15, 40, 8),
                StructureRotation.NONE,
                random(),
            ),
        )

        assertEquals("cinnabar", path(chunk.getBlockStateId(15, 40, 8)))
        assertEquals("air", path(chunk.getBlockStateId(0, 40, 8)))
        assertEquals("air", path(chunk.getBlockStateId(1, 40, 8)))
        val external = chunk.takeGeneratedBlockWrites().sortedBy { it.x }
        assertEquals(listOf(16, 17), external.map { it.x })
        assertTrue(external.all { it.replaceAnyState })
        assertTrue(external.all { path(it.stateId) == "cinnabar" })
    }

    private fun ceilingChunk(): Chunk =
        Chunk(0, 0).apply {
            for (x in 0 until 16) {
                for (z in 0 until 16) {
                    for (y in CEILING_BOTTOM..CEILING_TOP) {
                        setBlockStateId(x, y, z, Blocks.STONE.stateId)
                    }
                }
            }
        }

    private fun blockLookup(chunk: Chunk): (Int, Int, Int) -> Int =
        { x, y, z ->
            if (x shr 4 == chunk.chunkX && z shr 4 == chunk.chunkZ) {
                chunk.getBlockStateId(x and 15, y, z and 15)
            } else {
                val generated = chunk.getDecorationBlockStateId(x, y, z)
                when {
                    !Blocks.isAir(generated) -> generated
                    y in CEILING_BOTTOM..CEILING_TOP -> Blocks.STONE.stateId
                    else -> Blocks.AIR.stateId
                }
            }
        }

    private fun markerTemplate(): StructureTemplate = template(3, 1, 5, listOf(Triple(0, 0, 0)))

    private fun threeBlockTemplate(): StructureTemplate =
        template(3, 1, 1, listOf(Triple(0, 0, 0), Triple(1, 0, 0), Triple(2, 0, 0)))

    private fun template(
        sizeX: Int,
        sizeY: Int,
        sizeZ: Int,
        blocks: List<Triple<Int, Int, Int>>,
    ): StructureTemplate {
        val root = NbtCompound()
        root["size"] = NbtList(NbtType.INT, listOf(NbtInt(sizeX), NbtInt(sizeY), NbtInt(sizeZ)))
        root["palette"] =
            NbtList(
                NbtType.COMPOUND,
                listOf(NbtCompound().apply { putString("Name", "minecraft:cinnabar") }),
            )
        root["blocks"] =
            NbtList(
                NbtType.COMPOUND,
                blocks.map { (x, y, z) ->
                    NbtCompound().apply {
                        this["pos"] = NbtList(NbtType.INT, listOf(NbtInt(x), NbtInt(y), NbtInt(z)))
                        putInt("state", 0)
                    }
                },
            )
        return StructureTemplate().apply { load(root) }
    }

    private fun count(
        chunk: Chunk,
        blockPath: String,
    ): Int {
        var total = 0
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in ORIGIN.y..CEILING_TOP + 8) if (path(chunk.getBlockStateId(x, y, z)) == blockPath) total++
            }
        }
        return total
    }

    private fun path(stateId: Int): String? = Blocks.byStateId(stateId)?.block?.path

    private fun random(): WorldgenRandom = WorldgenRandom(XoroshiroRandom(1234L))

    private companion object {
        val ROOTED_SULFUR_ID = Identifier.vanilla("rooted_sulfur_spring")
        val ORIGIN = DecorationPos(8, 20, 8)
        const val CEILING_BOTTOM = 21
        const val CEILING_TOP = 29
    }
}
