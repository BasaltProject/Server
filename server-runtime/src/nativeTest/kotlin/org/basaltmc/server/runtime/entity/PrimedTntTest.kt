package org.basaltmc.server.runtime.entity

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.runtime.world.WorldState
import org.basaltmc.server.world.Chunk
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/** parity: `PrimedTnt`, the entity a lit block of TNT becomes. */
class PrimedTntTest {
    private fun worldWithFloor(): WorldState {
        val chunk = Chunk(0, 0)
        for (x in 0 until 16) {
            for (z in 0 until 16) chunk.setBlockStateId(x, 64, z, Blocks.STONE.stateId)
        }
        return WorldState(listOf(chunk))
    }

    private fun prime(
        world: WorldState,
        x: Int = 8,
        y: Int = 70,
        z: Int = 8,
        fuse: Int = PrimedTnt.DEFAULT_FUSE,
    ) = PrimedTnt.prime(world, x, y, z, entityId = 1, fuse = fuse, protocolType = 100)

    /** parity: `TntBlock.explode`, which puts it in the middle of the block the TNT stood in. */
    @Test
    fun itIsLitWhereTheBlockWas() {
        val world = worldWithFloor()

        val tnt = prime(world, x = 8, y = 70, z = 8)

        assertEquals(8.5, tnt.x)
        assertEquals(70.0, tnt.y)
        assertEquals(8.5, tnt.z)
        assertEquals(PrimedTnt.DEFAULT_FUSE, tnt.fuse, "four seconds of fuse")
        assertEquals(1, world.entities().size)
    }

    /** parity: the fuse is counted after the move, and the entity falls while it burns. */
    @Test
    fun itFallsWhileTheFuseBurns() {
        val world = worldWithFloor()
        val tnt = prime(world, y = 70)

        repeat(20) { assertFalse(PrimedTnt.tick(world, tnt)) }

        assertTrue(tnt.y < 70.0, "it should have fallen: ${tnt.y}")
        assertEquals(PrimedTnt.DEFAULT_FUSE - 20, tnt.fuse)
    }

    @Test
    fun itLandsOnTheFloorAndStays() {
        val world = worldWithFloor()
        val tnt = prime(world, y = 70)

        repeat(60) { PrimedTnt.tick(world, tnt) }

        assertEquals(65.0, tnt.y, 1e-9, "resting on top of the floor")
        assertTrue(tnt.onGround)
    }

    /** parity: `PrimedTnt.tick`, which discards the entity the tick its fuse reaches zero. */
    @Test
    fun theFuseRunsOutExactlyWhenVanillaSays() {
        val world = worldWithFloor()
        val tnt = prime(world, y = 66, fuse = 3)

        assertFalse(PrimedTnt.tick(world, tnt))
        assertFalse(PrimedTnt.tick(world, tnt))
        assertTrue(PrimedTnt.tick(world, tnt), "the third tick is the one that goes off")
    }

    /** parity: `multiply(0.7, -0.5, 0.7)` on landing, which is what a cannon's charge rides on. */
    @Test
    fun aLandingCostsItSidewaysSpeed() {
        val world = worldWithFloor()
        val tnt = prime(world, y = 66)
        tnt.velocityX = 1.0

        var landed = false
        repeat(20) {
            PrimedTnt.tick(world, tnt)
            if (tnt.onGround) landed = true
        }

        assertTrue(landed, "it never reached the floor")
        assertTrue(tnt.velocityX < 1.0, "the landing should have slowed it: ${tnt.velocityX}")
        assertTrue(tnt.x > 8.5, "and it still travelled: ${tnt.x}")
    }
}
