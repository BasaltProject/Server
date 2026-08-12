package org.basaltmc.server.entity.component

/** A per-tick velocity, in blocks/tick. Benchmark/prototype component, not tied to any real entity type yet. */
public data class Velocity(
    val dx: Double,
    val dy: Double,
    val dz: Double,
)
