package org.basaltmc.server.world.climate

import kotlin.native.concurrent.ThreadLocal

/** One slot per tree, per thread. Kotlin/Native has no per-instance thread local, so the tree brings its index. */
@ThreadLocal
private object ClimateSearchState {
    var slots: Array<Any?> = arrayOfNulls(INITIAL_SLOTS)

    private const val INITIAL_SLOTS = 4
}

internal actual fun lastClimateLeaf(slot: Int): Any? {
    val slots = ClimateSearchState.slots
    return if (slot < slots.size) slots[slot] else null
}

internal actual fun setLastClimateLeaf(
    slot: Int,
    leaf: Any?,
) {
    var slots = ClimateSearchState.slots
    if (slot >= slots.size) {
        slots = slots.copyOf(maxOf(slot + 1, slots.size * 2))
        ClimateSearchState.slots = slots
    }
    slots[slot] = leaf
}
