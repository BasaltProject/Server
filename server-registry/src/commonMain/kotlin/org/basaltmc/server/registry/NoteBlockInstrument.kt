package org.basaltmc.server.registry

import org.basaltmc.wire.common.Identifier
import kotlin.math.pow

/**
 * What a note block plays, which is not a property of the note block but of the block under it.
 *
 * parity: `NoteBlockInstrument`. The three kinds are what decide the rest: only a base block is
 * tunable, and only the others work *above* the note block instead of below it.
 */
public enum class NoteBlockInstrument(
    public val serializedName: String,
    public val sound: String,
    private val kind: Kind,
) {
    HARP("harp", "block.note_block.harp", Kind.BASE_BLOCK),
    BASEDRUM("basedrum", "block.note_block.basedrum", Kind.BASE_BLOCK),
    SNARE("snare", "block.note_block.snare", Kind.BASE_BLOCK),
    HAT("hat", "block.note_block.hat", Kind.BASE_BLOCK),
    BASS("bass", "block.note_block.bass", Kind.BASE_BLOCK),
    FLUTE("flute", "block.note_block.flute", Kind.BASE_BLOCK),
    BELL("bell", "block.note_block.bell", Kind.BASE_BLOCK),
    GUITAR("guitar", "block.note_block.guitar", Kind.BASE_BLOCK),
    CHIME("chime", "block.note_block.chime", Kind.BASE_BLOCK),
    XYLOPHONE("xylophone", "block.note_block.xylophone", Kind.BASE_BLOCK),
    IRON_XYLOPHONE("iron_xylophone", "block.note_block.iron_xylophone", Kind.BASE_BLOCK),
    COW_BELL("cow_bell", "block.note_block.cow_bell", Kind.BASE_BLOCK),
    DIDGERIDOO("didgeridoo", "block.note_block.didgeridoo", Kind.BASE_BLOCK),
    BIT("bit", "block.note_block.bit", Kind.BASE_BLOCK),
    BANJO("banjo", "block.note_block.banjo", Kind.BASE_BLOCK),
    PLING("pling", "block.note_block.pling", Kind.BASE_BLOCK),
    TRUMPET("trumpet", "block.note_block.trumpet", Kind.BASE_BLOCK),
    TRUMPET_EXPOSED("trumpet_exposed", "block.note_block.trumpet_exposed", Kind.BASE_BLOCK),
    TRUMPET_OXIDIZED("trumpet_oxidized", "block.note_block.trumpet_oxidized", Kind.BASE_BLOCK),
    TRUMPET_WEATHERED("trumpet_weathered", "block.note_block.trumpet_weathered", Kind.BASE_BLOCK),
    ZOMBIE("zombie", "block.note_block.imitate.zombie", Kind.MOB_HEAD),
    SKELETON("skeleton", "block.note_block.imitate.skeleton", Kind.MOB_HEAD),
    CREEPER("creeper", "block.note_block.imitate.creeper", Kind.MOB_HEAD),
    DRAGON("dragon", "block.note_block.imitate.ender_dragon", Kind.MOB_HEAD),
    WITHER_SKELETON("wither_skeleton", "block.note_block.imitate.wither_skeleton", Kind.MOB_HEAD),
    PIGLIN("piglin", "block.note_block.imitate.piglin", Kind.MOB_HEAD),
    CUSTOM_HEAD("custom_head", "ui.button.click", Kind.CUSTOM),
    ;

    /** parity: only a base block answers to the note, so a head always plays at pitch one. */
    public val isTunable: Boolean get() = kind == Kind.BASE_BLOCK

    /** parity: a head sits on top of the note block; everything else has to be under it. */
    public val worksAbove: Boolean get() = kind != Kind.BASE_BLOCK

    public enum class Kind { BASE_BLOCK, MOB_HEAD, CUSTOM }

    public companion object {
        private val byBlock: Map<Identifier, NoteBlockInstrument> =
            ALL_BLOCK_INSTRUMENTS.associate { (block, instrument) ->
                Identifier.vanilla(block) to entries.first { it.serializedName == instrument }
            }

        private val byName: Map<String, NoteBlockInstrument> = entries.associateBy { it.serializedName }

        /** What a note block over [block] plays. Anything not in the table is the harp. */
        public fun of(block: Identifier): NoteBlockInstrument = byBlock[block] ?: HARP

        public fun byName(name: String): NoteBlockInstrument? = byName[name]

        /** parity: `NoteBlock.getPitchFromNote`, an octave either side of the middle note. */
        public fun pitchOf(note: Int): Float = 2.0.pow((note - 12) / 12.0).toFloat()
    }
}
