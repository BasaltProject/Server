package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.NoteBlockInstrument
import org.basaltmc.server.runtime.config.WorldDeclaration
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.server.runtime.config.WorldSettings
import org.basaltmc.server.runtime.config.WorldType
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.logging.LogLevel
import org.basaltmc.wire.common.logging.Logger
import org.basaltmc.wire.protocol.play.SoundSource
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertTrue

/** parity: `Level.playSound`, `Level.blockEvent` and `Level.levelEvent` asked for by a block. */
class BlockEffectsTest {
    private val world =
        LoadedWorld(
            WorldDeclaration("effects", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            WorldSettings.VANILLA,
            WorldMetadata(0L, 0, 64, 0),
            "build/test-data/effects",
            Logger("BlockEffectsTest").apply { minLevel = LogLevel.ERROR },
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

    private fun sounds(effects: List<BlockEffect>) = effects.filterIsInstance<BlockEffect.Sound>()

    /** parity: `NoteBlock.neighborChanged`, which plays on the rising edge and nothing on the way down. */
    @Test
    fun aNoteBlockPlaysWhenTheSignalArrives() {
        world.state.setBlockState(
            2,
            70,
            2,
            state(
                "note_block",
                mapOf(
                    "instrument" to "harp",
                    "note" to "0",
                    "powered" to "false",
                ),
            ),
        )
        world.state.setBlockState(3, 70, 2, state("redstone_block"))

        val on = RedstoneReactors.update(world, 3, 70, 2).first { it.x == 2 }
        assertEquals("block.note_block.harp", sounds(on.effects).single().name)
        assertEquals(SoundSource.RECORDS, sounds(on.effects).single().source)
        assertTrue(on.effects.any { it is BlockEffect.Event }, "the swing, which is what draws the particle")

        world.state.setBlockState(3, 70, 2, Blocks.AIR.stateId)
        val off = RedstoneReactors.update(world, 3, 70, 2).first { it.x == 2 }
        assertTrue(off.effects.isEmpty(), "a signal going away is silent")
    }

    /** parity: `NoteBlock.playNote`, which will not sound with a solid block sitting on it. */
    @Test
    fun aCoveredNoteBlockStaysQuiet() {
        world.state.setBlockState(5, 70, 2, state("note_block", mapOf("instrument" to "harp", "powered" to "false")))
        world.state.setBlockState(5, 71, 2, state("stone"))
        world.state.setBlockState(6, 70, 2, state("redstone_block"))

        val change = RedstoneReactors.update(world, 6, 70, 2).first { it.x == 5 }
        assertTrue(change.effects.isEmpty())
    }

    /** parity: `NoteBlock.getPitchFromNote`, an octave either side of the twelfth note. */
    @Test
    fun theNoteDecidesThePitch() {
        assertEquals(1.0f, NoteBlockInstrument.pitchOf(12))
        assertEquals(0.5f, NoteBlockInstrument.pitchOf(0))
        assertEquals(2.0f, NoteBlockInstrument.pitchOf(24))
    }

    /** parity: `NoteBlock.setInstrument`, which reads the block under it, or a head over it. */
    @Test
    fun theBlockUnderneathDecidesTheInstrument() {
        world.state.setBlockState(8, 69, 2, state("oak_planks"))
        world.state.setBlockState(8, 70, 2, state("note_block"))
        assertEquals(NoteBlockInstrument.BASS, NoteBlocks.instrumentAt(world, 8, 70, 2))

        world.state.setBlockState(8, 69, 2, state("stone"))
        assertEquals(NoteBlockInstrument.BASEDRUM, NoteBlocks.instrumentAt(world, 8, 70, 2))

        // parity: a head works from above, where any ordinary block would be ignored.
        world.state.setBlockState(8, 71, 2, state("zombie_head", mapOf("rotation" to "0")))
        assertEquals(NoteBlockInstrument.ZOMBIE, NoteBlocks.instrumentAt(world, 8, 70, 2))
    }

    /** parity: `NoteBlock.updateShape`, which re-picks the instrument when what is under it changes. */
    @Test
    fun changingWhatIsUnderItRetunesIt() {
        world.state.setBlockState(11, 69, 2, state("stone"))
        world.state.setBlockState(11, 70, 2, state("note_block", mapOf("instrument" to "basedrum")))

        world.state.setBlockState(11, 69, 2, state("oak_planks"))
        val retuned = assertNotNull(NoteBlocks.retune(world, 11, 70, 2, world.state.getBlockState(11, 70, 2) ?: 0))

        assertEquals("bass", BlockStateDefinitions.propertiesOf(retuned)["instrument"])
    }

    /** parity: `BellBlock.attemptToRing`, which is a swing and a sound and no change of state. */
    @Test
    fun aBellRingsOnASignal() {
        world.state.setBlockState(
            14,
            70,
            2,
            state("bell", mapOf("facing" to "north", "attachment" to "floor", "powered" to "false")),
        )
        world.state.setBlockState(15, 70, 2, state("redstone_block"))

        val change = RedstoneReactors.update(world, 15, 70, 2).first { it.x == 14 }
        assertEquals("block.bell.use", sounds(change.effects).single().name)
        assertEquals(2f, sounds(change.effects).single().volume)
        val swing = change.effects.filterIsInstance<BlockEffect.Event>().single()
        assertEquals(1, swing.b0)
        assertEquals(2, swing.b1, "north, as `Direction.get3DDataValue` counts it")
    }

    /** parity: `CopperBulbBlock.checkAndFlip`, which only makes a noise when the light flips. */
    @Test
    fun aCopperBulbClicksWhenItFlips() {
        world.state.setBlockState(
            2,
            73,
            2,
            state("copper_bulb", mapOf("lit" to "false", "powered" to "false")),
        )
        world.state.setBlockState(3, 73, 2, state("redstone_block"))

        val on = RedstoneReactors.update(world, 3, 73, 2).first { it.x == 2 }
        assertEquals("block.copper_bulb.turn_on", sounds(on.effects).single().name)

        // The falling edge only clears `powered`, so there is nothing to hear.
        world.state.setBlockState(3, 73, 2, Blocks.AIR.stateId)
        val off = RedstoneReactors.update(world, 3, 73, 2).first { it.x == 2 }
        assertTrue(sounds(off.effects).isEmpty())
    }

    /** parity: `DoorBlock.playSound`, the same sound a hand makes on the same door. */
    @Test
    fun aDoorOpenedByRedstoneIsHeard() {
        world.state.setBlockState(
            5,
            73,
            2,
            state("oak_door", mapOf("half" to "lower", "facing" to "north", "open" to "false", "powered" to "false")),
        )
        world.state.setBlockState(6, 73, 2, state("redstone_block"))

        val change = RedstoneReactors.update(world, 6, 73, 2).first { it.x == 5 }
        assertEquals("minecraft:block.wooden_door.open", sounds(change.effects).single().name)
    }

    /** parity: `DispenserBlock.dispenseFrom`, which clicks at nothing when there is nothing in it. */
    @Test
    fun anEmptyDispenserClicks() {
        world.state.setBlockState(8, 73, 2, state("dispenser", mapOf("facing" to "up", "triggered" to "false")))
        val chunk = checkNotNull(world.state.chunkIfLoaded(0, 0))
        chunk.setBlockEntity(
            org.basaltmc.server.world
                .ChunkBlockEntity(
                    8,
                    73,
                    2,
                    Identifier.vanilla("dispenser"),
                    org.basaltmc.wire.nbt
                        .NbtCompound(),
                ),
        )

        val effects = mutableListOf<BlockEffect>()
        val fired =
            Dispensers.fire(
                world,
                8,
                73,
                2,
                org.basaltmc.server.random
                    .LegacyRandom(1L),
                effects,
            ) { 1 }

        assertEquals(false, fired)
        val click = effects.filterIsInstance<BlockEffect.Effect>().single()
        assertEquals(BlockEffect.Effect.DISPENSER_FAIL, click.type)
    }
}
