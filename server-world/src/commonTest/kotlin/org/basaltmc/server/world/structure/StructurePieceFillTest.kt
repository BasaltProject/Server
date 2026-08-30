package org.basaltmc.server.world.structure

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/**
 * The coordinate transform goes wrong quietly: a corridor built with the wrong handedness still looks
 * like a corridor. These pin the four orientations against the rule vanilla uses.
 */
class StructurePieceFillTest {
    private val box = StructureBoundingBox(10, 60, 20, 12, 62, 30)

    private fun fill(orientation: StructureDirection?) =
        StructurePieceFill(
            box = box,
            orientation = orientation,
            chunk = Chunk(0, 0),
            clip = StructureBoundingBox(-64, -64, -64, 320, 320, 320),
            oceanFloorHeight = { _, _ -> 70 },
        )

    @Test
    fun `a piece entered from the north runs its length towards lower z`() {
        val north = fill(StructureDirection.NORTH)
        // Local z counts away from the entrance, so z=0 is the far edge of the box.
        assertEquals(10, north.worldX(0, 0))
        assertEquals(30, north.worldZ(0, 0))
        assertEquals(20, north.worldZ(0, 10))
        assertEquals(60, north.worldY(0))
    }

    @Test
    fun `a piece entered from the south runs its length towards higher z`() {
        val south = fill(StructureDirection.SOUTH)
        assertEquals(10, south.worldX(0, 0))
        assertEquals(20, south.worldZ(0, 0))
        assertEquals(30, south.worldZ(0, 10))
    }

    @Test
    fun `an east-west piece swaps which local axis runs along it`() {
        val west = fill(StructureDirection.WEST)
        assertEquals(12, west.worldX(0, 0))
        assertEquals(10, west.worldX(0, 2))
        assertEquals(20, west.worldZ(0, 0))
        assertEquals(21, west.worldZ(1, 0))

        val east = fill(StructureDirection.EAST)
        assertEquals(10, east.worldX(0, 0))
        assertEquals(12, east.worldX(0, 2))
        assertEquals(20, east.worldZ(0, 0))
    }

    @Test
    fun `a piece with no orientation is written in world coordinates`() {
        val none = fill(null)
        assertEquals(3, none.worldX(3, 7))
        assertEquals(7, none.worldZ(3, 7))
        assertEquals(5, none.worldY(5))
    }

    @Test
    fun `a write outside the clip box is dropped`() {
        val chunk = Chunk(0, 0)
        val stone =
            Blocks.REGISTRY
                .byIdentifier(
                    org.basaltmc.wire.common.Identifier
                        .vanilla("stone"),
                )!!
                .stateId
        val piece =
            StructurePieceFill(
                box = StructureBoundingBox(0, 60, 0, 2, 62, 10),
                orientation = StructureDirection.SOUTH,
                chunk = chunk,
                // Only the first three blocks along the piece are in range.
                clip = StructureBoundingBox(0, 60, 0, 15, 62, 2),
                oceanFloorHeight = { _, _ -> 70 },
            )
        for (z in 0..10) piece.place(stone, 0, 0, z)
        assertEquals(stone, chunk.getBlockStateId(0, 60, 0))
        assertEquals(stone, chunk.getBlockStateId(0, 60, 2))
        assertTrue(Blocks.isAir(chunk.getBlockStateId(0, 60, 3)), "z=3 is past the clip and must stay air")
    }

    @Test
    fun `generateBox walls the shell and fills the inside`() {
        val chunk = Chunk(0, 0)
        val stone =
            Blocks.REGISTRY
                .byIdentifier(
                    org.basaltmc.wire.common.Identifier
                        .vanilla("stone"),
                )!!
                .stateId
        val piece =
            StructurePieceFill(
                box = StructureBoundingBox(0, 60, 0, 4, 64, 4),
                orientation = StructureDirection.SOUTH,
                chunk = chunk,
                clip = StructureBoundingBox(0, 0, 0, 15, 320, 15),
                oceanFloorHeight = { _, _ -> 70 },
            )
        piece.generateBox(0, 0, 0, 4, 4, 4, stone, Blocks.AIR.stateId, skipAir = false)
        assertEquals(stone, chunk.getBlockStateId(0, 60, 0), "a corner is shell")
        assertEquals(stone, chunk.getBlockStateId(2, 60, 2), "the floor is shell")
        assertTrue(Blocks.isAir(chunk.getBlockStateId(2, 62, 2)), "the middle is fill")
    }
}
