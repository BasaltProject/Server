package org.basaltmc.server.runtime.entity

import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.runtime.world.WorldState
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** parity: `PathFinder` over `WalkNodeEvaluator`. */
class PathFinderTest {
    private fun worldWithFloor(): WorldState {
        val chunk = Chunk(0, 0)
        for (x in 0 until 16) {
            for (z in 0 until 16) chunk.setBlockStateId(x, 64, z, Blocks.STONE.stateId)
        }
        return WorldState(listOf(chunk))
    }

    /** Across open ground the path is the straight line, one step per block. */
    @Test
    fun openGroundIsWalkedStraightAcross() {
        val world = worldWithFloor()

        val path = assertNotNull(PathFinder.find(world, 2, 65, 2, 8, 65, 2))

        assertEquals(6, path.size, "six steps east: ${path.map { it.toList() }}")
        assertEquals(listOf(8, 65, 2), path.last().toList())
    }

    /** A wall in the way is walked around rather than through. */
    @Test
    fun aWallIsWalkedAround() {
        val world = worldWithFloor()
        for (z in 0..6) {
            world.setBlockState(5, 65, z, Blocks.STONE.stateId)
            world.setBlockState(5, 66, z, Blocks.STONE.stateId)
        }

        val path = assertNotNull(PathFinder.find(world, 2, 65, 2, 8, 65, 2))

        assertTrue(path.none { it[0] == 5 && it[2] <= 6 }, "it went through the wall")
        assertEquals(listOf(8, 65, 2), path.last().toList())
    }

    /** parity: a step up of one is free, so a path climbs a single block without a jump. */
    @Test
    fun aSingleStepIsClimbed() {
        val world = worldWithFloor()
        for (z in 0 until 16) world.setBlockState(5, 65, z, Blocks.STONE.stateId)

        val path = assertNotNull(PathFinder.find(world, 2, 65, 2, 8, 65, 2))

        assertTrue(path.any { it[0] == 5 && it[1] == 66 }, "it should have stepped up onto the ledge")
    }

    /** parity: two blocks is too tall to step, so a wall right across has no way through. */
    @Test
    fun thereIsNoPathThroughASealedWall() {
        val world = worldWithFloor()
        for (z in 0 until 16) {
            world.setBlockState(5, 65, z, Blocks.STONE.stateId)
            world.setBlockState(5, 66, z, Blocks.STONE.stateId)
            world.setBlockState(5, 67, z, Blocks.STONE.stateId)
        }

        assertNull(PathFinder.find(world, 2, 65, 2, 8, 65, 2))
    }

    /** parity: the water malus, which is why a mob walks round a pond rather than through it. */
    @Test
    fun waterIsAvoidedWhenThereIsAWayRound() {
        val world = worldWithFloor()
        val water = checkNotNull(BlockStateDefinitions.stateIdOf(Identifier.vanilla("water"), mapOf("level" to "0")))
        for (z in 1..3) world.setBlockState(5, 65, z, water)

        val path = assertNotNull(PathFinder.find(world, 2, 65, 2, 8, 65, 2))

        assertTrue(path.none { it[0] == 5 && it[2] in 1..3 }, "it waded through: ${path.map { it.toList() }}")
    }

    /** Standing where it already is needs no path at all. */
    @Test
    fun thereIsNothingToWalkWhenItIsAlreadyThere() {
        val world = worldWithFloor()

        assertEquals(emptyList(), PathFinder.find(world, 3, 65, 3, 3, 65, 3))
    }

    @Test
    fun whatCountsAsSomewhereToStand() {
        val world = worldWithFloor()

        assertTrue(PathFinder.canStandAt(world, 4, 65, 4))
        assertTrue(!PathFinder.canStandAt(world, 4, 66, 4), "nothing under it")
        world.setBlockState(4, 66, 4, Blocks.STONE.stateId)
        assertTrue(!PathFinder.canStandAt(world, 4, 65, 4), "no room for its head")
    }
}
