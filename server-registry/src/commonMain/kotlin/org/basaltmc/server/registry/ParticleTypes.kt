package org.basaltmc.server.registry

import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.protocol.play.ParticleOptionShape

/** One particle type: its id, its protocol number, and the shape of the options it carries. */
public data class ParticleType(
    public val id: Identifier,
    public val protocolId: Int,
    public val optionShape: ParticleOptionShape,
) {
    public val needsOptions: Boolean get() = optionShape != ParticleOptionShape.NONE
}

/**
 * The `minecraft:particle_type` registry, from [ALL_PARTICLE_TYPE_IDS], plus which option codec each
 * type uses.
 *
 * parity: the shapes come from `ParticleTypes`' own registrations in the jar. Getting one wrong is
 * not a cosmetic mistake: the options are written straight after the type id with no length, so a
 * type that writes the wrong shape desynchronises the client on the very next packet.
 */
public object ParticleTypes {
    private val SHAPES: Map<String, ParticleOptionShape> =
        mapOf(
            "block" to ParticleOptionShape.BLOCK_STATE,
            "block_marker" to ParticleOptionShape.BLOCK_STATE,
            "falling_dust" to ParticleOptionShape.BLOCK_STATE,
            "dust_pillar" to ParticleOptionShape.BLOCK_STATE,
            "block_crumble" to ParticleOptionShape.BLOCK_STATE,
            "entity_effect" to ParticleOptionShape.COLOR,
            "flash" to ParticleOptionShape.COLOR,
            "tinted_leaves" to ParticleOptionShape.COLOR,
            "effect" to ParticleOptionShape.SPELL,
            "instant_effect" to ParticleOptionShape.SPELL,
            "dust" to ParticleOptionShape.DUST,
            "dust_color_transition" to ParticleOptionShape.DUST_TRANSITION,
            "item" to ParticleOptionShape.ITEM,
            "trail" to ParticleOptionShape.TRAIL,
            "sculk_charge" to ParticleOptionShape.NUMBER,
            "dragon_breath" to ParticleOptionShape.NUMBER,
            "shriek" to ParticleOptionShape.DELAY,
            "geyser" to ParticleOptionShape.GEYSER,
            "geyser_plume" to ParticleOptionShape.GEYSER,
            "geyser_base" to ParticleOptionShape.GEYSER_BASE,
            "geyser_poof" to ParticleOptionShape.GEYSER_BASE,
            "vibration" to ParticleOptionShape.VIBRATION,
        )

    private val byName: Map<Identifier, ParticleType> =
        ALL_PARTICLE_TYPE_IDS.associate { (protocolId, path) ->
            val id = Identifier.vanilla(path)
            id to ParticleType(id, protocolId, SHAPES[path] ?: ParticleOptionShape.NONE)
        }

    public val all: List<ParticleType> get() = byName.values.sortedBy { it.protocolId }

    public operator fun get(id: Identifier): ParticleType? = byName[id]
}
