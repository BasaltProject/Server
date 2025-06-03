package org.basaltmc.server.entity.ecs

/**
 * A sparse-set component store: dense, cache-friendly iteration over every entity that has this
 * component, plus O(1) lookup/insert/remove by entity index. `sparse[entityIndex]` is either the
 * position of that entity in the dense arrays, or [ABSENT]; `dense[i]`/`values[i]` are always kept
 * packed with no gaps, `remove` doing so by swapping the removed slot with the last one instead of
 * shifting. Keyed by raw entity *index*, not the full [EntityId] (the generation check happens once
 * in [World], not on every component access).
 */
public class ComponentStore<T> {
    private var sparse = IntArray(0)

    @PublishedApi
    internal var dense: IntArray = IntArray(0)

    @PublishedApi
    internal var values: Array<Any?> = arrayOfNulls(0)

    @PublishedApi
    internal var count: Int = 0

    public val size: Int get() = count

    public fun has(entityIndex: Int): Boolean = entityIndex < sparse.size && sparse[entityIndex] != ABSENT

    public fun get(entityIndex: Int): T? {
        if (!has(entityIndex)) return null
        @Suppress("UNCHECKED_CAST")
        return values[sparse[entityIndex]] as T
    }

    public fun set(
        entityIndex: Int,
        value: T,
    ) {
        ensureSparseCapacity(entityIndex + 1)
        if (has(entityIndex)) {
            values[sparse[entityIndex]] = value
            return
        }
        ensureDenseCapacity(count + 1)
        sparse[entityIndex] = count
        dense[count] = entityIndex
        values[count] = value
        count++
    }

    public fun remove(entityIndex: Int) {
        if (!has(entityIndex)) return
        val slot = sparse[entityIndex]
        val lastSlot = count - 1
        val lastEntity = dense[lastSlot]
        dense[slot] = lastEntity
        values[slot] = values[lastSlot]
        sparse[lastEntity] = slot
        values[lastSlot] = null
        sparse[entityIndex] = ABSENT
        count--
    }

    public inline fun forEach(action: (entityIndex: Int, value: T) -> Unit) {
        for (i in 0 until count) {
            @Suppress("UNCHECKED_CAST")
            action(dense[i], values[i] as T)
        }
    }

    private fun ensureSparseCapacity(minSize: Int) {
        if (minSize <= sparse.size) return
        val newSize = maxOf(minSize, sparse.size * 2 + 16)
        val newSparse = IntArray(newSize) { ABSENT }
        sparse.copyInto(newSparse)
        sparse = newSparse
    }

    private fun ensureDenseCapacity(minSize: Int) {
        if (minSize <= dense.size) return
        val newSize = maxOf(minSize, dense.size * 2 + 16)
        dense = dense.copyOf(newSize)
        values = values.copyOf(newSize)
    }

    private companion object {
        const val ABSENT = -1
    }
}
