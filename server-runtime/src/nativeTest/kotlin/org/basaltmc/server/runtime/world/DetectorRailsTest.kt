package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockFace
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.runtime.config.WorldDeclaration
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.server.runtime.config.WorldSettings
import org.basaltmc.server.runtime.config.WorldType
import org.basaltmc.server.runtime.entity.ServerEntity
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.logging.LogLevel
import org.basaltmc.wire.common.logging.Logger
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/** parity: `DetectorRailBlock.checkPressed`. */
@OptIn(ExperimentalUuidApi::class)
class DetectorRailsTest {
    private val world =
        LoadedWorld(
            WorldDeclaration("rails", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            WorldSettings.VANILLA,
            WorldMetadata(0L, 0, 64, 0),
            "build/test-data/detector-rails",
            Logger("DetectorRailsTest").apply { minLevel = LogLevel.ERROR },
            1,
        )

    init {
        world.state.getOrGenerateChunk(0, 0)
    }

    @AfterTest
    fun stopGenerating() {
        world.stopGenerating(drainTimeoutMillis = 100)
    }

    private fun railAt(
        x: Int,
        y: Int,
        z: Int,
    ) {
        world.state.setBlockState(
            x,
            y,
            z,
            checkNotNull(
                BlockStateDefinitions.stateIdOf(
                    Identifier.vanilla("detector_rail"),
                    mapOf("shape" to "north_south", "powered" to "false", "waterlogged" to "false"),
                ),
            ),
        )
    }

    private fun minecart(
        x: Double,
        y: Double,
        z: Double,
        type: String = "minecart",
    ) = ServerEntity(1, Uuid.random(), Identifier.vanilla(type), 1, x, y, z)

    private fun poweredAt(
        x: Int,
        y: Int,
        z: Int,
    ) = BlockStateDefinitions.propertiesOf(world.state.getBlockState(x, y, z) ?: 0)["powered"]

    /** parity: a cart standing on the rail powers it, and books the twenty ticks to look again. */
    @Test
    fun aMinecartOnItPowersIt() {
        railAt(4, 70, 4)

        val change = assertNotNull(DetectorRails.check(world, 4, 70, 4, listOf(minecart(4.5, 70.0, 4.5))))

        assertEquals("true", poweredAt(4, 70, 4))
        assertEquals("true", BlockStateDefinitions.propertiesOf(change.stateId)["powered"])
        val pending =
            world.state
                .chunkIfLoaded(0, 0)
                ?.scheduledBlockTicks()
                .orEmpty()
        assertTrue(pending.any { it.localX == 4 && it.type.path == "detector_rail" && it.delay == 20 })
    }

    @Test
    fun anEmptyRailStaysDark() {
        railAt(6, 70, 6)

        assertNull(DetectorRails.check(world, 6, 70, 6, emptyList()))
        assertEquals("false", poweredAt(6, 70, 6))
    }

    /** The cart rolling off is what the scheduled recheck is for. */
    @Test
    fun theCartRollingOffLetsItGo() {
        railAt(8, 70, 8)
        DetectorRails.check(world, 8, 70, 8, listOf(minecart(8.5, 70.0, 8.5)))

        assertNotNull(DetectorRails.check(world, 8, 70, 8, emptyList()))

        assertEquals("false", poweredAt(8, 70, 8))
    }

    /** parity: `AbstractMinecart`, of which every kind presses the rail. */
    @Test
    fun anyKindOfCartCounts() {
        assertTrue(DetectorRails.isMinecart(Identifier.vanilla("minecart")))
        assertTrue(DetectorRails.isMinecart(Identifier.vanilla("chest_minecart")))
        assertTrue(DetectorRails.isMinecart(Identifier.vanilla("tnt_minecart")))
        assertTrue(!DetectorRails.isMinecart(Identifier.vanilla("cow")), "an animal is not a cart")
    }

    /** A cow standing on it is not a minecart, so the rail stays dark. */
    @Test
    fun somethingElseOnItDoesNothing() {
        railAt(10, 70, 10)

        assertNull(DetectorRails.check(world, 10, 70, 10, listOf(minecart(10.5, 70.0, 10.5, type = "cow"))))
        assertEquals("false", poweredAt(10, 70, 10))
    }

    /** parity: `DetectorRailBlock.getSignal`, which is what the redstone beside it reads. */
    @Test
    fun aPressedRailFeedsRedstone() {
        railAt(12, 70, 12)
        DetectorRails.check(world, 12, 70, 12, listOf(minecart(12.5, 70.0, 12.5)))

        val stateId = checkNotNull(world.state.getBlockState(12, 70, 12))
        assertEquals(15, Redstone.signalOf(stateId, BlockFace.NORTH))
        assertEquals(15, Redstone.directSignalOf(stateId, BlockFace.UP), "strongly into the block below")
        assertEquals(0, Redstone.directSignalOf(stateId, BlockFace.NORTH))
    }
}
