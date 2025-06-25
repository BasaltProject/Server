package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockFace
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.runtime.config.WorldDeclaration
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.server.runtime.config.WorldSettings
import org.basaltmc.server.runtime.config.WorldType
import org.basaltmc.server.runtime.entity.BlockHit
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.logging.LogLevel
import org.basaltmc.wire.common.logging.Logger
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** parity: the `onProjectileHit` overrides, and `TargetBlock` above all. */
class ProjectileHitsTest {
    private val world =
        LoadedWorld(
            WorldDeclaration("hits", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            WorldSettings.VANILLA,
            WorldMetadata(0L, 0, 64, 0),
            "build/test-data/hits",
            Logger("ProjectileHitsTest").apply { minLevel = LogLevel.ERROR },
            1,
        )

    init {
        world.state.getOrGenerateChunk(0, 0)
    }

    @AfterTest
    fun stopGenerating() {
        world.stopGenerating(drainTimeoutMillis = 100)
    }

    private fun state(
        name: String,
        properties: Map<String, String> = emptyMap(),
    ): Int = checkNotNull(BlockStateDefinitions.stateIdOf(Identifier.vanilla(name), properties))

    private fun powerAt(
        x: Int,
        y: Int,
        z: Int,
    ): Int = BlockStateDefinitions.propertiesOf(world.state.getBlockState(x, y, z) ?: 0)["power"]?.toIntOrNull() ?: -1

    private fun hitOn(
        x: Int,
        y: Int,
        z: Int,
        offsetX: Double,
        offsetZ: Double,
    ) = BlockHit(x + offsetX, y + 1.0, z + offsetZ, x, y, z, BlockFace.UP)

    /** parity: `getRedstoneStrength`: dead centre is fifteen and the rim is one. */
    @Test
    fun aTargetReadsWhereItWasHit() {
        world.state.setBlockState(3, 70, 3, state("target", mapOf("power" to "0")))

        val change = assertNotNull(ProjectileHits.onHit(world, hitOn(3, 70, 3, 0.5, 0.5), byArrow = true))

        assertEquals(15, powerAt(3, 70, 3), "the middle of the face is fifteen")
        assertEquals(15, BlockStateDefinitions.propertiesOf(change.stateId)["power"]?.toInt())
    }

    @Test
    fun theRimIsWorthLess() {
        world.state.setBlockState(4, 70, 3, state("target", mapOf("power" to "0")))

        ProjectileHits.onHit(world, hitOn(4, 70, 3, 0.95, 0.5), byArrow = true)

        val power = powerAt(4, 70, 3)
        assertTrue(power in 1..3, "a hit on the rim should be weak, was $power")
    }

    /** parity: twenty ticks for an arrow, eight for anything thrown. */
    @Test
    fun theSignalIsHeldForTheRightTime() {
        world.state.setBlockState(5, 70, 3, state("target", mapOf("power" to "0")))
        ProjectileHits.onHit(world, hitOn(5, 70, 3, 0.5, 0.5), byArrow = true)

        val pending =
            world.state
                .chunkIfLoaded(0, 0)
                ?.scheduledBlockTicks()
                .orEmpty()
        assertTrue(
            pending.any { it.localX == 5 && it.y == 70 && it.type.path == "target" && it.delay == 20 },
            "an arrow holds it for twenty: $pending",
        )

        world.state.setBlockState(6, 70, 3, state("target", mapOf("power" to "0")))
        ProjectileHits.onHit(world, hitOn(6, 70, 3, 0.5, 0.5), byArrow = false)
        assertTrue(
            world.state
                .chunkIfLoaded(0, 0)
                ?.scheduledBlockTicks()
                .orEmpty()
                .any { it.localX == 6 && it.delay == 8 },
            "and a snowball for eight",
        )
    }

    /** parity: a target already counting down keeps what it has. */
    @Test
    fun aSecondHitWhileItIsLitChangesNothing() {
        world.state.setBlockState(7, 70, 3, state("target", mapOf("power" to "0")))
        ProjectileHits.onHit(world, hitOn(7, 70, 3, 0.5, 0.5), byArrow = true)

        assertNull(ProjectileHits.onHit(world, hitOn(7, 70, 3, 0.95, 0.5), byArrow = true))
        assertEquals(15, powerAt(7, 70, 3))
    }

    @Test
    fun theScheduledTickLetsItGo() {
        world.state.setBlockState(8, 70, 3, state("target", mapOf("power" to "9")))

        val released = assertNotNull(ProjectileHits.releaseTarget(world, 8, 70, 3))

        assertEquals(0, powerAt(8, 70, 3))
        assertEquals(0, BlockStateDefinitions.propertiesOf(released.stateId)["power"]?.toInt())
        assertNull(ProjectileHits.releaseTarget(world, 8, 70, 3), "and once dark it stays dark")
    }

    /** parity: `ChorusFlowerBlock.onProjectileHit` and `DecoratedPotBlock`'s own: they break. */
    @Test
    fun someBlocksSimplyBreak() {
        world.state.setBlockState(9, 70, 3, state("chorus_flower", mapOf("age" to "0")))

        assertNotNull(ProjectileHits.onHit(world, hitOn(9, 70, 3, 0.5, 0.5), byArrow = true))

        assertEquals(
            "air",
            BlockStateDefinitions.blockOf(world.state.getBlockState(9, 70, 3) ?: 0)?.path,
        )
    }

    /** parity: `BigDripleafBlock.onProjectileHit`, which tips it right over. */
    @Test
    fun aDripleafTipsOver() {
        world.state.setBlockState(
            10,
            70,
            3,
            state(
                "big_dripleaf",
                mapOf("facing" to "north", "tilt" to "none", "waterlogged" to "false"),
            ),
        )

        assertNotNull(ProjectileHits.onHit(world, hitOn(10, 70, 3, 0.5, 0.5), byArrow = true))

        assertEquals(
            "full",
            BlockStateDefinitions.propertiesOf(world.state.getBlockState(10, 70, 3) ?: 0)["tilt"],
        )
    }
}
