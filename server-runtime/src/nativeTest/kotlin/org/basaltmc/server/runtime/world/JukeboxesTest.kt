package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockFace
import org.basaltmc.server.registry.BlockStateDefinitions
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
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** parity: `JukeboxBlock` and the song data 26.2 keeps in `jukebox_song`. */
class JukeboxesTest {
    private val world =
        LoadedWorld(
            WorldDeclaration("jukebox", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            WorldSettings.VANILLA,
            WorldMetadata(0L, 0, 64, 0),
            "build/test-data/jukebox",
            Logger("JukeboxesTest").apply { minLevel = LogLevel.ERROR },
            1,
        )

    init {
        world.state.getOrGenerateChunk(0, 0)
    }

    @AfterTest
    fun stopGenerating() {
        world.stopGenerating(drainTimeoutMillis = 100)
    }

    private fun jukeboxAt(
        x: Int,
        y: Int,
        z: Int,
    ) {
        world.state.setBlockState(
            x,
            y,
            z,
            checkNotNull(
                BlockStateDefinitions.stateIdOf(Identifier.vanilla("jukebox"), mapOf("has_record" to "false")),
            ),
        )
    }

    private fun hasRecordAt(
        x: Int,
        y: Int,
        z: Int,
    ) = BlockStateDefinitions.propertiesOf(world.state.getBlockState(x, y, z) ?: 0)["has_record"]

    @Test
    fun aDiscGoesInAndComesBackOut() {
        jukeboxAt(4, 70, 4)

        assertNotNull(Jukeboxes.insert(world, 4, 70, 4, Identifier.vanilla("music_disc_13")))
        assertEquals("true", hasRecordAt(4, 70, 4))
        assertEquals("13", Jukeboxes.songAt(world, 4, 70, 4))

        assertNull(
            Jukeboxes.insert(world, 4, 70, 4, Identifier.vanilla("music_disc_cat")),
            "one at a time",
        )

        val (change, disc) = assertNotNull(Jukeboxes.eject(world, 4, 70, 4))
        assertEquals("false", hasRecordAt(4, 70, 4))
        assertEquals(Identifier.vanilla("music_disc_13"), disc, "it gives back what it was given")
        assertEquals("false", BlockStateDefinitions.propertiesOf(change.stateId)["has_record"])
        assertNull(Jukeboxes.eject(world, 4, 70, 4), "and an empty one gives nothing")
    }

    /** parity: `JukeboxBlock.ownSignal`, fifteen while it plays and nothing when it does not. */
    @Test
    fun aPlayingJukeboxFeedsRedstone() {
        jukeboxAt(6, 70, 6)
        assertEquals(0, Redstone.signalOf(checkNotNull(world.state.getBlockState(6, 70, 6)), BlockFace.NORTH))

        Jukeboxes.insert(world, 6, 70, 6, Identifier.vanilla("music_disc_cat"))

        assertEquals(15, Redstone.signalOf(checkNotNull(world.state.getBlockState(6, 70, 6)), BlockFace.NORTH))
    }

    @Test
    fun whatCountsAsADisc() {
        assertTrue(Jukeboxes.isDisc(Identifier.vanilla("music_disc_pigstep")))
        assertTrue(!Jukeboxes.isDisc(Identifier.vanilla("stone")))
        assertEquals("pigstep", Jukeboxes.songOf(Identifier.vanilla("music_disc_pigstep")))
    }

    /** Without the jar there is no song data, so a comparator reads nothing rather than guessing. */
    @Test
    fun withoutTheJarThereIsNoComparatorOutput() {
        assertEquals(0, Jukeboxes(serverJar = null).comparatorOutputOf("13"))
    }

    /** parity: `getAnalogOutputSignal`, the number of the song rather than how full the block is. */
    @Test
    fun aComparatorReadsTheSongNumber() {
        jukeboxAt(8, 70, 8)
        Jukeboxes.insert(world, 8, 70, 8, Identifier.vanilla("music_disc_11"), comparatorOutput = 11)

        assertEquals(11, Containers.analogSignal(world, 8, 70, 8))

        Jukeboxes.eject(world, 8, 70, 8)
        assertEquals(0, Containers.analogSignal(world, 8, 70, 8), "an empty jukebox reads nothing")
    }
}
