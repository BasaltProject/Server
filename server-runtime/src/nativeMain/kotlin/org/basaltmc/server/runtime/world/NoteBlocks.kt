package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.NoteBlockInstrument
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.protocol.play.SoundSource

/**
 * The note block: which instrument it plays and when it plays it.
 *
 * parity: `NoteBlock`. The instrument is not chosen when the note is played, it is a property of the
 * state, written when the block is placed and rewritten whenever what is above or below it changes.
 */
public object NoteBlocks {
    public val BLOCK: Identifier = Identifier.vanilla("note_block")

    /** parity: `NoteBlock.NOTE_VOLUME`, loud enough to carry. */
    private const val NOTE_VOLUME = 3.0f

    /** parity: `NoteBlock.NOTE`, two octaves. */
    private const val NOTES = 25

    /** parity: `NoteBlock.setInstrument`: a head on top wins, and otherwise the block underneath. */
    public fun instrumentAt(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): NoteBlockInstrument {
        val above = NoteBlockInstrument.of(blockAt(world, x, y + 1, z) ?: Blocks.AIR.block)
        if (above.worksAbove) return above
        val below = NoteBlockInstrument.of(blockAt(world, x, y - 1, z) ?: Blocks.AIR.block)
        return if (below.worksAbove) NoteBlockInstrument.HARP else below
    }

    /** parity: `NoteBlock.updateShape` on the Y axis, which is the only one that matters here. */
    public fun retune(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
    ): Int? {
        if (BlockStateDefinitions.blockOf(stateId) != BLOCK) return null
        val wanted = instrumentAt(world, x, y, z)
        if (BlockStateDefinitions.propertiesOf(stateId)["instrument"] == wanted.serializedName) return null
        val tuned =
            BlockStateDefinitions.stateIdOf(BLOCK, mapOf("instrument" to wanted.serializedName), stateId) ?: return null
        if (!world.state.setBlockState(x, y, z, tuned)) return null
        return tuned
    }

    /** parity: `NoteBlock.getStateForPlacement`, which picks the instrument before the block lands. */
    public fun placed(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
    ): Int =
        if (BlockStateDefinitions.blockOf(stateId) != BLOCK) {
            stateId
        } else {
            BlockStateDefinitions.stateIdOf(
                BLOCK,
                mapOf("instrument" to instrumentAt(world, x, y, z).serializedName),
                stateId,
            ) ?: stateId
        }

    /**
     * parity: `NoteBlock.playNote`: a note block with something solid on top stays quiet, unless the
     * instrument is the kind that lives up there in the first place.
     */
    public fun playNote(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
    ): List<BlockEffect> {
        val properties = BlockStateDefinitions.propertiesOf(stateId)
        val instrument = NoteBlockInstrument.byName(properties["instrument"] ?: "") ?: NoteBlockInstrument.HARP
        val above = world.state.getBlockState(x, y + 1, z) ?: return emptyList()
        if (!instrument.worksAbove && !Blocks.isAir(above)) return emptyList()
        val effects = mutableListOf<BlockEffect>(BlockEffect.Event(b0 = 0, b1 = 0))
        // parity: `triggerEvent` gives up on a head whose own sound it cannot find, and we never
        // have one: the swing goes out, the sound does not.
        if (instrument == NoteBlockInstrument.CUSTOM_HEAD) return effects
        val pitch =
            if (instrument.isTunable) NoteBlockInstrument.pitchOf(properties["note"]?.toIntOrNull() ?: 0) else 1.0f
        effects += BlockEffect.Sound(instrument.sound, SoundSource.RECORDS, NOTE_VOLUME, pitch)
        return effects
    }

    /** parity: `NoteBlock.useWithoutItem`, which moves the note on one and plays it. */
    public fun tune(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): BlockChange? {
        val stateId = world.state.getBlockState(x, y, z) ?: return null
        if (BlockStateDefinitions.blockOf(stateId) != BLOCK) return null
        val note = (BlockStateDefinitions.propertiesOf(stateId)["note"]?.toIntOrNull() ?: 0) + 1
        val tuned =
            BlockStateDefinitions.stateIdOf(BLOCK, mapOf("note" to (note % NOTES).toString()), stateId) ?: return null
        if (!world.state.setBlockState(x, y, z, tuned)) return null
        return BlockChange(x, y, z, tuned, playNote(world, x, y, z, tuned))
    }

    /** parity: `NoteBlock.attack`, which plays the note without changing it. */
    public fun strike(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): List<BlockEffect> {
        val stateId = world.state.getBlockState(x, y, z) ?: return emptyList()
        if (BlockStateDefinitions.blockOf(stateId) != BLOCK) return emptyList()
        return playNote(world, x, y, z, stateId)
    }

    private fun blockAt(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Identifier? = world.state.getBlockState(x, y, z)?.let { BlockStateDefinitions.blockOf(it) }
}
