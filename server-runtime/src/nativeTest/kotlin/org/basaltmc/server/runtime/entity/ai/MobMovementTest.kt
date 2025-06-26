package org.basaltmc.server.runtime.entity.ai

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.runtime.entity.ServerEntity
import org.basaltmc.server.runtime.world.WorldState
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier
import kotlin.math.sqrt
import kotlin.test.Test
import kotlin.test.assertTrue
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/**
 * parity: `Mob.setSpeed` feeding `zza`, which `moveRelative` then scales again: what a mob
 * accelerates by is the speed times itself, and a cow settles at about a seventh of a block a tick.
 */
@OptIn(ExperimentalUuidApi::class)
class MobMovementTest {
    private fun world(): WorldState {
        val chunks =
            listOf(Chunk(0, 0), Chunk(1, 0)).onEach { chunk ->
                for (x in 0 until 16) {
                    for (z in 0 until 16) chunk.setBlockStateId(x, 64, z, Blocks.STONE.stateId)
                }
            }
        return WorldState(chunks)
    }

    private fun cow(world: WorldState): ServerEntity =
        world
            .spawnEntity(ServerEntity(1, Uuid.random(), Identifier.vanilla("cow"), 20, 8.5, 65.0, 8.5))
            .also { it.onGround = true }

    @Test
    fun aCowWalksAtAboutAVanillaCowsPace() {
        val world = world()
        val cow = cow(world)
        val speed = 0.25

        var top = 0.0
        repeat(60) {
            MoveControl.setWanted(cow, 20.0, 65.0, 8.5, speedModifier = 1.0)
            MoveControl.tick(world, cow, speed)
            top = maxOf(top, sqrt(cow.velocityX * cow.velocityX + cow.velocityZ * cow.velocityZ))
        }

        // parity: 0.25 of speed accelerates by 0.25 * 0.25, and the 0.6 * 0.91 of drag applies to
        // the whole of it, which settles at 0.0625 * 0.546 / (1 - 0.546): a block and a half a
        // second, which is what a cow walks at.
        assertTrue(top in 0.070..0.080, "a cow settled at $top blocks a tick")
    }

    @Test
    fun itKeepsItsFeetOnFlatGround() {
        val world = world()
        val cow = cow(world)
        var airborne = 0
        repeat(40) {
            MoveControl.setWanted(cow, 20.0, 65.0, 8.5, speedModifier = 1.0)
            MoveControl.tick(world, cow, 0.25)
            if (!cow.onGround) airborne++
        }
        // parity: `Mth.equal`'s slack. Comparing exactly called a rounding error a wall, and the mob
        // hopped over it every tick.
        assertTrue(airborne <= 1, "it left the ground $airborne times crossing flat stone")
    }

    @Test
    fun aMobWithNowhereToGoStaysPut() {
        val world = world()
        val cow = cow(world)
        repeat(20) { MoveControl.tick(world, cow, 0.25) }
        assertTrue(cow.velocityX == 0.0 && cow.velocityZ == 0.0)
        assertTrue(cow.x == 8.5 && cow.z == 8.5)
    }

    @Test
    fun theHeadTurnsTowardsWhatItIsLookingAt() {
        val world = world()
        val cow = cow(world)
        LookControl.lookAt(cow, cow.x + 5.0, cow.y, cow.z)
        repeat(20) {
            LookControl.lookAt(cow, cow.x + 5.0, cow.y, cow.z)
            LookControl.tick(cow)
        }
        // Looking along positive X is a yaw of -90 in Minecraft's own frame.
        assertTrue(cow.headYaw in -95f..-85f, "the head ended at ${cow.headYaw}")
    }
}
