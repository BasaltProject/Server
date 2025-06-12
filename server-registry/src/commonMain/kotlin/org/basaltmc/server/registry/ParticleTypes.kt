package org.basaltmc.server.registry

import org.basaltmc.wire.common.Identifier

/** One particle type: its id, its protocol number, and whether it needs options written after it. */
public data class ParticleType(
    public val id: Identifier,
    public val protocolId: Int,
    public val needsOptions: Boolean,
)

/** The `minecraft:particle_type` registry, from [ALL_PARTICLE_TYPE_IDS]. */
public object ParticleTypes {
    private val byName: Map<Identifier, ParticleType> =
        ALL_PARTICLE_TYPE_IDS.associate { (protocolId, path) ->
            val id = Identifier.vanilla(path)
            id to ParticleType(id, protocolId, needsOptions = path in PARTICLES_WITH_OPTIONS)
        }

    public val all: List<ParticleType> get() = byName.values.sortedBy { it.protocolId }

    public operator fun get(id: Identifier): ParticleType? = byName[id]
}
