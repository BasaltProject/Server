package org.basaltmc.server.world.structure

import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.region.ChunkNbt
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

/**
 * The index answers "which structure is at this block" for spawn overrides, so a start that
 * survives a save but loses its pieces, or a full-bounds hit reported as a piece hit, changes
 * what spawns without changing anything visible.
 */
class StructureIndexTest {
    private fun box(
        minX: Int,
        minY: Int,
        minZ: Int,
        maxX: Int,
        maxY: Int,
        maxZ: Int,
    ) = StructureBoundingBox(minX, minY, minZ, maxX, maxY, maxZ)

    private val piece = box(4, 40, 4, 11, 47, 11)
    private val record =
        StructureStartRecord("fortress", 0, 0, box(0, 32, 0, 31, 60, 31), listOf(piece))

    @Test
    fun `a start survives the round trip with its pieces`() {
        val chunk = Chunk(0, 0)
        chunk.structures.add(record)

        val restored = ChunkNbt.read(ChunkNbt.write(chunk), chunk.verticalBounds)
        val starts = restored.structures.starts()
        assertEquals(1, starts.size)
        assertEquals(record, starts.single())
    }

    @Test
    fun `a chunk with no structure writes nothing to carry back`() {
        val chunk = Chunk(3, -2)
        assertTrue(chunk.structures.isEmpty)
        val restored = ChunkNbt.read(ChunkNbt.write(chunk), chunk.verticalBounds)
        assertTrue(restored.structures.isEmpty)
    }

    @Test
    fun `full bounds and piece bounds are not the same question`() {
        val chunk = Chunk(0, 0)
        chunk.structures.add(record)
        val queries = StructureQueries { x, z -> if (x == 0 && z == 0) chunk.structures else null }

        val inPiece = StructurePos(5, 41, 5)
        assertNotNull(queries.startAt(inPiece, "fortress"))
        assertNotNull(queries.startWithPieceAt(inPiece, "fortress"))

        // Inside the fortress as a whole but in none of its pieces: the nether-brick gate sees it,
        // the piece-bounds monster override does not.
        val betweenPieces = StructurePos(2, 35, 2)
        assertNotNull(queries.startAt(betweenPieces, "fortress"))
        assertNull(queries.startWithPieceAt(betweenPieces, "fortress"))
    }

    @Test
    fun `a query never reaches a chunk the lookup refuses`() {
        val chunk = Chunk(0, 0)
        chunk.structures.add(record)
        val queries = StructureQueries { _, _ -> null }
        assertNull(queries.startAt(StructurePos(5, 41, 5), "fortress"))
        assertTrue(queries.startsIn(StructurePos(5, 41, 5)).isEmpty())
    }

    @Test
    fun `the same start offered twice by neighbouring passes is recorded once`() {
        val chunk = Chunk(0, 0)
        chunk.structures.add(record)
        chunk.structures.add(record)
        assertEquals(1, chunk.structures.starts().size)
    }

    @Test
    fun `every structure with an override declares which box it tests`() {
        // The three without a generator yet are here so that wiring one up needs no new table.
        for (structure in StructureSpawnOverrides.STRUCTURES) {
            val monster = StructureSpawnOverrides.of(structure, "monster")
            assertNotNull(monster, "$structure has no monster override")
        }
        assertEquals(
            StructureSpawnOverrides.Bounds.PIECE,
            StructureSpawnOverrides.of("fortress", "monster")?.bounds,
        )
        assertEquals(
            StructureSpawnOverrides.Bounds.FULL,
            StructureSpawnOverrides.of("pillager_outpost", "monster")?.bounds,
        )
        // An empty list silences the category; it is not the absence of an override.
        assertEquals(emptyList(), StructureSpawnOverrides.of("ancient_city", "monster")?.spawns)
        assertNull(StructureSpawnOverrides.of("ancient_city", "nonexistent_category"))
    }
}
