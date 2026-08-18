package org.basaltmc.server.registry

import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals

/** parity: `Block.getFriction`, `getSpeedFactor` and `getJumpFactor`, from the jar's own classes. */
class BlockMovementTest {
    private fun stateOf(name: String): Int =
        checkNotNull(BlockStateDefinitions.stateIdOf(Identifier.vanilla(name), emptyMap()))

    @Test
    fun anOrdinaryBlockIsTheDefault() {
        assertEquals(0.6f, BlockMovement.frictionOf(stateOf("stone")))
        assertEquals(1.0f, BlockMovement.speedFactorOf(stateOf("stone")))
        assertEquals(1.0f, BlockMovement.jumpFactorOf(stateOf("stone")))
    }

    @Test
    fun iceIsSlippery() {
        assertEquals(0.98f, BlockMovement.frictionOf(stateOf("ice")))
        assertEquals(0.98f, BlockMovement.frictionOf(stateOf("packed_ice")))
        assertEquals(0.989f, BlockMovement.frictionOf(stateOf("blue_ice")), "blue ice is the slipperiest")
        assertEquals(0.8f, BlockMovement.frictionOf(stateOf("slime_block")))
    }

    /** parity: `getBounceRestitution`: slime throws everything back, a bed three quarters of it. */
    @Test
    fun slimeAndBedsBounce() {
        assertEquals(1.0f, BlockMovement.bouncinessOf(stateOf("slime_block")))
        assertEquals(0.75f, BlockMovement.bouncinessOf(stateOf("red_bed")))
        assertEquals(0.0f, BlockMovement.bouncinessOf(stateOf("stone")))
    }

    @Test
    fun soulSandAndHoneySlowWhatWalksOnThem() {
        assertEquals(0.4f, BlockMovement.speedFactorOf(stateOf("soul_sand")))
        assertEquals(0.4f, BlockMovement.speedFactorOf(stateOf("honey_block")))
        assertEquals(0.5f, BlockMovement.jumpFactorOf(stateOf("honey_block")), "and honey kills the jump too")
        assertEquals(1.0f, BlockMovement.jumpFactorOf(stateOf("soul_sand")))
    }
}
