package org.basaltmc.entity.ecs

/**
 * Owns entity id lifecycle: allocation (reusing destroyed indices before growing), and bumping an
 * index's generation on destroy so a stale [EntityId] compares dead even after its slot is recycled
 * for a new entity. Doesn't own or know about component data: that lives in whichever
 * [ComponentStore]s the caller composes alongside a [World], deliberately, so no single storage
 * shape is locked in before the entity architecture is actually decided.
 */
public class World {
    private var generations = IntArray(0)
    private var alive = BooleanArray(0)
    private val freeIndices = ArrayDeque<Int>()
    private var nextIndex = 0

    public fun create(): EntityId {
        val index =
            if (freeIndices.isNotEmpty()) {
                freeIndices.removeFirst()
            } else {
                val allocated = nextIndex
                ensureCapacity(allocated + 1)
                nextIndex++
                allocated
            }
        alive[index] = true
        return EntityId.of(index, generations[index])
    }

    public fun destroy(id: EntityId) {
        if (!isAlive(id)) return
        val index = id.index
        alive[index] = false
        generations[index]++
        freeIndices.addLast(index)
    }

    public fun isAlive(id: EntityId): Boolean =
        id.index < alive.size && alive[id.index] && generations[id.index] == id.generation

    private fun ensureCapacity(minSize: Int) {
        if (minSize <= generations.size) return
        val newSize = maxOf(minSize, generations.size * 2 + 16)
        generations = generations.copyOf(newSize)
        alive = alive.copyOf(newSize)
    }
}
