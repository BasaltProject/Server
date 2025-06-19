package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.runtime.config.WorldDeclaration
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.server.runtime.config.WorldSettings
import org.basaltmc.server.runtime.config.WorldType
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.logging.LogLevel
import org.basaltmc.wire.common.logging.Logger
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/** parity: `SpongeBlock.onPlace`, the sponge that drinks the pool it lands in. */
class SpongesTest {
    private val world =
        LoadedWorld(
            WorldDeclaration("sponge", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            WorldSettings.VANILLA,
            WorldMetadata(0L, 0, 64, 0),
            "build/test-data/sponge",
            Logger("SpongesTest").apply { minLevel = LogLevel.ERROR },
            1,
        )

    init {
        world.state.getOrGenerateChunk(0, 0)
    }

    @AfterTest
    fun stopGenerating() {
        world.stopGenerating(drainTimeoutMillis = 100)
    }

    private fun pathAt(
        x: Int,
        y: Int,
        z: Int,
    ): String? = BlockStateDefinitions.blockOf(world.state.getBlockState(x, y, z) ?: 0)?.path

    @Test
    fun itDrinksThePoolAndTurnsWet() {
        for (x in 0 until 8) {
            for (z in 0 until 8) {
                for (y in 70 until 73) world.state.setBlockState(x, y, z, Blocks.WATER.stateId)
            }
        }
        world.state.setBlockState(4, 71, 4, checkNotNull(Blocks.REGISTRY.idOf(Identifier.vanilla("sponge"))))

        val changed = mutableListOf<Triple<Int, Int, Int>>()
        Sponges.absorb(world, 4, 71, 4) { _, x, y, z -> changed += Triple(x, y, z) }

        assertEquals("wet_sponge", pathAt(4, 71, 4), "a sponge that drank anything turns wet")
        assertEquals(Blocks.AIR.stateId, world.state.getBlockState(4, 72, 4), "the water right above is gone")
        // parity: sixty-five blocks at most, the sponge itself counted among what changed.
        assertTrue(changed.size <= 66, "it drank ${changed.size} blocks")
        assertEquals("water", pathAt(0, 70, 0), "and it does not reach the far corner of the pool")
    }

    /** parity: `BucketPickup.pickupBlock`: a logged block is drained, not broken. */
    @Test
    fun itDrainsALoggedBlockInsteadOfBreakingIt() {
        val logged =
            checkNotNull(
                BlockStateDefinitions.stateIdOf(
                    Identifier.vanilla("oak_slab"),
                    mapOf("waterlogged" to "true", "type" to "bottom"),
                ),
            )
        world.state.setBlockState(10, 71, 10, logged)
        world.state.setBlockState(11, 71, 10, checkNotNull(Blocks.REGISTRY.idOf(Identifier.vanilla("sponge"))))

        Sponges.absorb(world, 11, 71, 10) { _, _, _, _ -> }

        assertEquals("oak_slab", pathAt(10, 71, 10), "the slab stays")
        assertEquals(
            "false",
            BlockStateDefinitions.propertiesOf(world.state.getBlockState(10, 71, 10) ?: 0)["waterlogged"],
            "but the water in it is gone",
        )
    }

    /** parity: `WetSpongeBlock.onPlace`: the Nether dries a wet sponge the moment it lands. */
    @Test
    fun theNetherDriesAWetSponge() {
        val nether =
            LoadedWorld(
                WorldDeclaration("sponge_nether", WorldDimension.NETHER, WorldType.VOID, seed = 0L, isDefault = false),
                WorldSettings.VANILLA,
                WorldMetadata(0L, 0, 64, 0),
                "build/test-data/sponge-nether",
                Logger("SpongesNetherTest").apply { minLevel = LogLevel.ERROR },
                1,
            )
        nether.state.getOrGenerateChunk(0, 0)
        nether.state.setBlockState(2, 71, 2, checkNotNull(Blocks.REGISTRY.idOf(Identifier.vanilla("wet_sponge"))))

        Sponges.dryIfEvaporating(nether, 2, 71, 2) { _, _, _, _ -> }

        assertEquals(
            "sponge",
            BlockStateDefinitions.blockOf(nether.state.getBlockState(2, 71, 2) ?: 0)?.path,
        )
        nether.stopGenerating(drainTimeoutMillis = 100)
    }

    /** A sponge in the dry does nothing at all. */
    @Test
    fun aDrySpongeStaysDry() {
        world.state.setBlockState(13, 71, 13, checkNotNull(Blocks.REGISTRY.idOf(Identifier.vanilla("sponge"))))

        Sponges.absorb(world, 13, 71, 13) { _, _, _, _ -> }

        assertEquals("sponge", pathAt(13, 71, 13))
    }
}
