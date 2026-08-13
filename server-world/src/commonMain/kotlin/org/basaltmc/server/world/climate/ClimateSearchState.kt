package org.basaltmc.server.world.climate

/**
 * // parity: the `ThreadLocal<Leaf<T>>` Climate.RTree keeps its previous result in. Seeding a
 * search with the last leaf this thread found prunes far harder, since consecutive lookups are
 * almost always neighbouring positions with the same answer.
 *
 * Per thread, and null until that thread has searched once, exactly as vanilla's is. [slot]
 * identifies one tree, standing in for vanilla's per-instance field.
 */
internal expect fun lastClimateLeaf(slot: Int): Any?

internal expect fun setLastClimateLeaf(
    slot: Int,
    leaf: Any?,
)
