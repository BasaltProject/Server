package org.basaltmc.server.runtime.entity

import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.runtime.world.WorldState
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier
import kotlin.math.abs
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/** parity: `OldMinecartBehavior.moveAlongTrack`. */
@OptIn(ExperimentalUuidApi::class)
class MinecartsTest {
    private fun world(): WorldState {
        val chunks =
            listOf(Chunk(0, 0), Chunk(1, 0)).onEach { chunk ->
                for (x in 0 until 16) {
                    for (z in 0 until 16) chunk.setBlockStateId(x, 64, z, Blocks.STONE.stateId)
                }
            }
        return WorldState(chunks)
    }

    private fun railLine(
        world: WorldState,
        from: Int,
        to: Int,
        z: Int = 8,
        block: String = "rail",
        powered: Boolean = false,
    ) {
        val properties =
            if (block == "rail") {
                mapOf("shape" to "east_west", "waterlogged" to "false")
            } else {
                mapOf("shape" to "east_west", "powered" to powered.toString(), "waterlogged" to "false")
            }
        val stateId = checkNotNull(BlockStateDefinitions.stateIdOf(Identifier.vanilla(block), properties))
        for (x in from..to) world.setBlockState(x, 65, z, stateId)
    }

    private fun cart(
        world: WorldState,
        x: Double = 4.5,
        z: Double = 8.5,
    ) = world.spawnEntity(
        ServerEntity(1, Uuid.random(), Identifier.vanilla("minecart"), 1, x, 65.0, z),
    )

    /** parity: a cart on a straight rail keeps rolling, losing 0.96 of its speed a tick. */
    @Test
    fun aCartRollsAlongTheRail() {
        val world = world()
        railLine(world, 2, 14)
        val cart = cart(world)
        cart.velocityX = 0.3

        repeat(5) { assertTrue(Minecarts.moveAlongTrack(world, cart)) }

        assertTrue(cart.x > 5.5, "it should have rolled east: ${cart.x}")
        assertEquals(8.5, cart.z, 1e-9, "and stayed on the rail")
        assertEquals(65.0, cart.y, 1e-9)
        assertTrue(cart.velocityX < 0.3, "and slowed down: ${cart.velocityX}")
    }

    /** parity: the cart is pinned to the middle of the rail, whatever it was pushed sideways by. */
    @Test
    fun itIsPinnedToTheLine() {
        val world = world()
        railLine(world, 2, 14)
        val cart = cart(world, z = 8.9)
        cart.velocityX = 0.1
        cart.velocityZ = 0.5

        Minecarts.moveAlongTrack(world, cart)

        assertEquals(8.5, cart.z, 1e-9, "sideways speed does not take it off the rail")
    }

    /** parity: a powered rail adds 0.06 a tick, so a cart on one speeds up rather than slowing. */
    @Test
    fun aPoweredRailPushesItAlong() {
        val world = world()
        railLine(world, 2, 14, block = "powered_rail", powered = true)
        val cart = cart(world)
        cart.velocityX = 0.1

        repeat(10) { Minecarts.moveAlongTrack(world, cart) }

        assertTrue(cart.velocityX > 0.1, "a powered rail should have sped it up: ${cart.velocityX}")
    }

    /** parity: an unpowered powered rail is a brake: it halves the speed and stops it under 0.03. */
    @Test
    fun anUnpoweredRailStopsIt() {
        val world = world()
        railLine(world, 2, 14, block = "powered_rail", powered = false)
        val cart = cart(world)
        cart.velocityX = 0.3

        repeat(10) { Minecarts.moveAlongTrack(world, cart) }

        assertEquals(0.0, cart.velocityX, "it should have stopped dead")
    }

    /** parity: the cap of 0.4 a tick, however hard a cart is pushed. */
    @Test
    fun itNeverGoesFasterThanTheCap() {
        val world = world()
        railLine(world, 2, 14, block = "powered_rail", powered = true)
        val cart = cart(world)
        cart.velocityX = 5.0

        val before = cart.x
        Minecarts.moveAlongTrack(world, cart)

        assertTrue(cart.x - before <= Minecarts.MAX_SPEED + 1e-9, "it moved ${cart.x - before} in one tick")
    }

    /** Off the rails there is nothing to follow, and the caller lets it fall. */
    @Test
    fun withoutARailItIsNotOnATrack() {
        val world = world()
        val cart = cart(world, x = 4.5)

        assertFalse(Minecarts.moveAlongTrack(world, cart))
    }

    /** parity: a powered rail with a block beside it nudges a still cart off the wall. */
    @Test
    fun aPoweredRailNudgesAStillCart() {
        val world = world()
        railLine(world, 2, 14, block = "powered_rail", powered = true)
        world.setBlockState(1, 65, 8, Blocks.STONE.stateId)
        val cart = cart(world, x = 2.5)

        repeat(20) { Minecarts.moveAlongTrack(world, cart) }

        assertTrue(cart.x > 4.5, "it should have been nudged east: ${cart.x}")
    }

    /** parity: a corner turns the cart, which is what a curved rail is for. */
    @Test
    fun aCornerTurnsIt() {
        val world = world()
        railLine(world, 2, 7)
        val corner =
            checkNotNull(
                BlockStateDefinitions.stateIdOf(
                    Identifier.vanilla("rail"),
                    mapOf("shape" to "south_west", "waterlogged" to "false"),
                ),
            )
        world.setBlockState(8, 65, 8, corner)
        for (z in 9..14) {
            world.setBlockState(
                8,
                65,
                z,
                checkNotNull(
                    BlockStateDefinitions.stateIdOf(
                        Identifier.vanilla("rail"),
                        mapOf("shape" to "north_south", "waterlogged" to "false"),
                    ),
                ),
            )
        }
        val cart = cart(world, x = 4.5)
        cart.velocityX = 0.3

        repeat(30) { Minecarts.moveAlongTrack(world, cart) }

        assertTrue(abs(cart.x - 8.5) < 0.6, "it should have turned the corner: ${cart.x}, ${cart.z}")
        assertTrue(cart.z > 8.5, "and gone south: ${cart.z}")
    }
}
