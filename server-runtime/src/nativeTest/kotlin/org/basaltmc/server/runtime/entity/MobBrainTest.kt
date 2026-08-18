package org.basaltmc.server.runtime.entity

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.runtime.world.WorldState
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier
import kotlin.math.abs
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/** parity: `RandomStrollGoal` over `DefaultRandomPos.getPos`, and the `MoveControl` that walks it. */
@OptIn(ExperimentalUuidApi::class)
class MobBrainTest {
    private fun worldWithFloor(): WorldState {
        val chunks =
            listOf(Chunk(0, 0), Chunk(0, 1), Chunk(1, 0), Chunk(1, 1)).onEach { chunk ->
                for (x in 0 until 16) {
                    for (z in 0 until 16) chunk.setBlockStateId(x, 64, z, Blocks.STONE.stateId)
                }
            }
        return WorldState(chunks)
    }

    private fun cow(
        world: WorldState,
        x: Double = 8.5,
        z: Double = 8.5,
    ): ServerEntity =
        world.spawnEntity(
            ServerEntity(
                entityId = 1,
                uuid = Uuid.random(),
                type = Identifier.vanilla("cow"),
                protocolType = 20,
                x = x,
                y = 65.0,
                z = z,
            ),
        )

    /** parity: `getWalkTargetValue`: a spot has to be standable, with room to stand in. */
    @Test
    fun itOnlyWandersSomewhereItCanStand() {
        val world = worldWithFloor()
        val mob = cow(world)

        repeat(50) {
            val target = MobBrain.strollTarget(world, mob, LegacyRandom(it.toLong())) ?: return@repeat
            val ground = world.getBlockState(target[0].toInt(), (target[1] - 1).toInt(), target[2].toInt())
            assertEquals(Blocks.STONE.stateId, ground, "it picked a spot with nothing under it")
        }
    }

    /** parity: `RandomStrollGoal`, one roll in 120: a mob does eventually set off. */
    @Test
    fun itSetsOffOnItsOwn() {
        val world = worldWithFloor()
        val mob = cow(world)
        val random = LegacyRandom(7)

        repeat(600) { MobBrain.tick(world, mob, random, speed = 0.2) }

        assertTrue(
            abs(mob.x - 8.5) > 0.5 || abs(mob.z - 8.5) > 0.5,
            "in thirty seconds it should have wandered somewhere: ${mob.x}, ${mob.z}",
        )
    }

    /** It walks towards where it is going, and stops when it gets there. */
    @Test
    fun itWalksToItsTargetAndStops() {
        val world = worldWithFloor()
        val mob = cow(world, x = 8.5, z = 8.5)
        mob.walkTarget = doubleArrayOf(8.5, 65.0, 16.5)

        var arrived = false
        repeat(200) {
            MobBrain.tick(world, mob, LegacyRandom(1), speed = 0.2)
            if (mob.walkTarget == null) arrived = true
        }

        assertTrue(arrived, "it never arrived: ${mob.z}")
        assertTrue(mob.z > 15.0, "and it should be near the target: ${mob.z}")
    }

    /** parity: `MoveControl`, which turns towards the target rather than snapping to it. */
    @Test
    fun itTurnsTowardsWhereItIsGoing() {
        val world = worldWithFloor()
        val mob = cow(world)
        mob.yaw = 0f
        mob.walkTarget = doubleArrayOf(20.5, 65.0, 8.5)

        MobBrain.tick(world, mob, LegacyRandom(1), speed = 0.2)
        val afterOne = mob.yaw

        assertTrue(abs(afterOne) <= 30f, "it turns at most thirty degrees a tick, turned $afterOne")
        repeat(10) { MobBrain.tick(world, mob, LegacyRandom(1), speed = 0.2) }
        assertEquals(-90f, mob.yaw, 1f, "and ends up facing east")
    }

    /** A mob with nowhere to go stands still rather than drifting. */
    @Test
    fun withoutATargetItStaysPut() {
        val world = worldWithFloor()
        val mob = cow(world)
        // A random that never rolls the one in 120 that starts a stroll.
        val stillRandom = LegacyRandom(3)
        mob.walkTarget = null

        repeat(5) { MobBrain.tick(world, mob, stillRandom, speed = 0.2) }

        assertEquals(8.5, mob.x, 1e-9)
        assertEquals(8.5, mob.z, 1e-9)
    }

    @Test
    fun whatCountsAsAMob() {
        assertTrue(Mobs.isMob(Identifier.vanilla("cow")))
        assertTrue(Mobs.isMob(Identifier.vanilla("zombie")))
        assertTrue(!Mobs.isMob(Identifier.vanilla("item")), "a dropped item has no brain")
        assertTrue(!Mobs.isMob(Identifier.vanilla("tnt")))
        assertEquals("monster", Mobs.categoryOf(Identifier.vanilla("creeper")))
    }

    @Test
    fun aStrollTargetIsNullWhenThereIsNowhereToGo() {
        val world = WorldState(listOf(Chunk(0, 0)))
        val mob = cow(world)

        assertNull(MobBrain.strollTarget(world, mob, LegacyRandom(1)), "an empty void offers nothing")
        assertNotNull(mob)
    }

    /** parity: `PathNavigation`, which is what takes a mob round a wall instead of into it. */
    @Test
    fun itWalksRoundAWall() {
        val world = worldWithFloor()
        for (z in 0..12) {
            world.setBlockState(12, 65, z, Blocks.STONE.stateId)
            world.setBlockState(12, 66, z, Blocks.STONE.stateId)
        }
        val mob = cow(world, x = 8.5, z = 8.5)
        mob.walkTarget = doubleArrayOf(16.5, 65.0, 8.5)
        mob.path =
            checkNotNull(PathFinder.find(world, 8, 65, 8, 16, 65, 8)) { "there should be a way round" }

        repeat(400) { MobBrain.tick(world, mob, LegacyRandom(1), speed = 0.25) }

        assertTrue(mob.x > 12.0, "it never got past the wall: ${mob.x}, ${mob.z}")
    }
}
