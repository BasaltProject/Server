package org.basaltmc.server.runtime.entity

import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.runtime.world.WorldState
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/** parity: `LivingEntity.travelInAir` and the step-up half of `Entity.collide`. */
@OptIn(ExperimentalUuidApi::class)
class LivingMovementTest {
    /** parity: `Attributes.MOVEMENT_SPEED`'s own default for a walking mob. */
    private val walkSpeed = 0.23

    /** Four chunks of floor, so twenty ticks of walking never runs off the edge of the world. */
    private fun worldFlooredWith(block: String = "stone"): WorldState {
        val stateId =
            checkNotNull(BlockStateDefinitions.stateIdOf(Identifier.vanilla(block), emptyMap()))
        val chunks =
            listOf(Chunk(0, 0), Chunk(0, 1), Chunk(1, 0), Chunk(1, 1)).onEach { chunk ->
                for (x in 0 until 16) {
                    for (z in 0 until 16) chunk.setBlockStateId(x, 64, z, stateId)
                }
            }
        return WorldState(chunks)
    }

    private var nextId = 1

    private fun mob(
        world: WorldState,
        x: Double = 8.5,
        y: Double = 65.0,
        z: Double = 8.5,
        yaw: Float = 0f,
    ): ServerEntity =
        world.spawnEntity(
            ServerEntity(
                entityId = nextId++,
                uuid = Uuid.random(),
                type = Identifier.vanilla("cow"),
                protocolType = 20,
                x = x,
                y = y,
                z = z,
                yaw = yaw,
            ),
        )

    /** Standing still on the ground stays still, tick after tick. */
    @Test
    fun standingStillStaysStill() {
        val world = worldFlooredWith()
        val cow = mob(world)

        repeat(10) { LivingMovement.travel(world, cow, forward = 0.0, strafe = 0.0, speed = walkSpeed) }

        assertEquals(8.5, cow.x)
        assertEquals(65.0, cow.y, 1e-9, "resting on the floor")
        assertTrue(cow.onGround)
    }

    /** parity: `getInputVector`, which turns "forward" into world axes through the yaw. */
    @Test
    fun itWalksTheWayItIsFacing() {
        val world = worldFlooredWith()
        val south = mob(world, yaw = 0f)
        val west = mob(world, yaw = 90f)

        repeat(20) {
            LivingMovement.travel(world, south, forward = 1.0, strafe = 0.0, speed = walkSpeed)
            LivingMovement.travel(world, west, forward = 1.0, strafe = 0.0, speed = walkSpeed)
        }

        assertTrue(south.z > 8.5, "yaw zero walks south: ${south.z}")
        assertEquals(8.5, south.x, 1e-6, "and nowhere else")
        assertTrue(west.x < 8.5, "yaw ninety walks west: ${west.x}")
    }

    /** parity: the friction cancellation, which is what makes ice slide. */
    @Test
    fun itSlidesOnIce() {
        val onStone = distanceAfterStopping("stone")
        val onIce = distanceAfterStopping("ice")

        assertTrue(onIce > onStone * 1.5, "ice should carry it further: $onIce against $onStone")
    }

    private fun distanceAfterStopping(block: String): Double {
        val world = worldFlooredWith(block)
        val cow = mob(world)
        repeat(20) { LivingMovement.travel(world, cow, forward = 1.0, strafe = 0.0, speed = walkSpeed) }
        val walked = cow.z
        repeat(20) { LivingMovement.travel(world, cow, forward = 0.0, strafe = 0.0, speed = walkSpeed) }
        return cow.z - walked
    }

    /** parity: the step height, which is what lets a mob walk onto a slab without jumping. */
    @Test
    fun itWalksUpASlab() {
        val world = worldFlooredWith()
        val slab =
            checkNotNull(
                BlockStateDefinitions.stateIdOf(
                    Identifier.vanilla("stone_slab"),
                    mapOf("type" to "bottom", "waterlogged" to "false"),
                ),
            )
        for (x in 0 until 16) {
            for (z in 12 until 20) world.setBlockState(x, 65, z, slab)
        }
        val cow = mob(world, z = 8.5)

        repeat(20) { LivingMovement.travel(world, cow, forward = 1.0, strafe = 0.0, speed = walkSpeed) }

        assertTrue(cow.z > 12.0, "it should have climbed onto the slab: ${cow.z}")
        assertEquals(65.5, cow.y, 1e-6, "standing on top of it")
    }

    /** A full block is too tall to step onto, so it walks into it and stops. */
    @Test
    fun aWholeBlockStopsIt() {
        val world = worldFlooredWith()
        for (x in 0 until 16) world.setBlockState(x, 65, 12, Blocks.STONE.stateId)
        val cow = mob(world, z = 8.5)

        repeat(40) { LivingMovement.travel(world, cow, forward = 1.0, strafe = 0.0, speed = walkSpeed) }

        assertTrue(cow.z < 12.0, "it walked through a wall: ${cow.z}")
        assertTrue(cow.horizontallyBlocked, "and it knows it is blocked")
        assertEquals(65.0, cow.y, 1e-9)
    }

    /** parity: `jumpFromGround`, and that a jump only happens from the ground. */
    @Test
    fun itJumpsFromTheGroundOnly() {
        val world = worldFlooredWith()
        val cow = mob(world)
        // Two ticks to land: the first has no downward speed at all, so nothing collides yet.
        repeat(2) { LivingMovement.travel(world, cow, 0.0, 0.0, walkSpeed) }

        LivingMovement.jump(world, cow)
        assertEquals(LivingMovement.JUMP_POWER, cow.velocityY, 1e-9)

        val before = cow.velocityY
        LivingMovement.jump(world, cow)
        assertEquals(before, cow.velocityY, "a second jump in mid-air does nothing")

        LivingMovement.travel(world, cow, 0.0, 0.0, walkSpeed)
        assertFalse(cow.onGround, "and it is off the ground now")
        assertTrue(cow.y > 65.0)
    }
}
