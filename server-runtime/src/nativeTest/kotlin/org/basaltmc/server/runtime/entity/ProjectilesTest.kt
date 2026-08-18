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

/** parity: `Projectile.shoot`, `AbstractArrow.tick` and `ThrowableProjectile.tick`. */
class ProjectilesTest {
    private fun world(): WorldState = WorldState(listOf(Chunk(0, 0)))

    private fun arrow(
        world: WorldState,
        x: Double = 1.5,
        y: Double = 70.0,
        z: Double = 1.5,
        dirX: Double = 1.0,
        dirY: Double = 0.0,
        dirZ: Double = 0.0,
        uncertainty: Float = 0f,
        kind: Projectiles.Kind = Projectiles.ARROW,
    ) = Projectiles.spawn(
        world,
        kind,
        entityId = 1,
        protocolType = 2,
        x = x,
        y = y,
        z = z,
        dirX = dirX,
        dirY = dirY,
        dirZ = dirZ,
        power = Projectiles.DISPENSER_POWER,
        uncertainty = uncertainty,
        random = LegacyRandom(1),
    )

    /** parity: `shoot`: the direction is normalised and then scaled by the power. */
    @Test
    fun itLeavesAtTheSpeedItWasShot() {
        val shot = arrow(world())

        assertEquals(Projectiles.DISPENSER_POWER.toDouble(), shot.velocityX, 1e-9)
        assertEquals(0.0, shot.velocityY)
        assertEquals(0.0, shot.velocityZ)
        assertEquals(90f, shot.yaw, 1e-3f, "east, pointing the way it is going")
    }

    /** parity: the triangular spread, which is what makes a dispenser miss. */
    @Test
    fun uncertaintyPushesItOffTheLine() {
        val straight = arrow(world())
        val scattered = arrow(world(), uncertainty = Projectiles.DISPENSER_UNCERTAINTY)

        assertTrue(
            abs(scattered.velocityY - straight.velocityY) > 1e-6 ||
                abs(scattered.velocityZ - straight.velocityZ) > 1e-6,
            "six degrees of uncertainty should move it off the line",
        )
    }

    /** parity: gravity, which pulls an arrow down harder than a snowball. */
    @Test
    fun anArrowFallsFasterThanASnowball() {
        val world = world()
        val shot = arrow(world, dirX = 1.0)
        val thrown = arrow(world, kind = Projectiles.SNOWBALL, dirX = 1.0)

        repeat(10) {
            Projectiles.tick(world, shot, Projectiles.ARROW)
            Projectiles.tick(world, thrown, Projectiles.SNOWBALL)
        }

        assertTrue(shot.y < thrown.y, "the arrow should be lower: ${shot.y} against ${thrown.y}")
    }

    /** parity: `AbstractArrow.setInGround`, which stops it dead in the block it hit. */
    @Test
    fun anArrowSticksInWhatItHits() {
        val world = world()
        world.setBlockState(6, 70, 1, Blocks.STONE.stateId)
        val shot = arrow(world, x = 1.5, y = 70.5, z = 1.5)

        var hit: BlockHit? = null
        repeat(20) { if (hit == null) hit = Projectiles.tick(world, shot, Projectiles.ARROW) }

        val landed = assertNotNull(hit, "it never reached the wall")
        assertEquals(6, landed.blockX)
        assertEquals(0.0, shot.velocityX, "it stops dead")
        assertTrue(shot.onGround, "and stays stuck")
        assertNull(Projectiles.tick(world, shot, Projectiles.ARROW), "a stuck arrow does not move again")
    }

    /** parity: a thrown item is gone on impact, which the caller does; the hit is what it answers. */
    @Test
    fun aSnowballReportsWhatItHit() {
        val world = world()
        world.setBlockState(4, 70, 1, Blocks.STONE.stateId)
        val thrown = arrow(world, x = 1.5, y = 70.5, z = 1.5, kind = Projectiles.SNOWBALL)

        var hit: BlockHit? = null
        repeat(20) { if (hit == null) hit = Projectiles.tick(world, thrown, Projectiles.SNOWBALL) }

        assertNotNull(hit)
        assertEquals(4, hit?.blockX)
    }

    @Test
    fun aDispenserKnowsWhatItShoots() {
        assertEquals(Projectiles.ARROW, Projectiles.dispensed(Identifier.vanilla("arrow")))
        assertEquals(Projectiles.SNOWBALL, Projectiles.dispensed(Identifier.vanilla("snowball")))
        assertEquals(Projectiles.EGG, Projectiles.dispensed(Identifier.vanilla("brown_egg")))
        assertNull(Projectiles.dispensed(Identifier.vanilla("stone")), "an ordinary item is thrown, not shot")
    }
}
