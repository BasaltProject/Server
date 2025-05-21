package org.basaltmc.entity.ecs

/**
 * A dense array index (the low 32 bits) packed with a generation counter (the high 32 bits): when
 * an index is recycled after its entity is destroyed, the generation bumps, so a stale [EntityId]
 * held elsewhere compares unequal to the new entity occupying that slot instead of silently
 * aliasing it. No allocation: a `value class` over a single [Long].
 */
public value class EntityId(
    public val raw: Long,
) {
    public val index: Int get() = (raw and 0xFFFFFFFFL).toInt()
    public val generation: Int get() = (raw ushr 32).toInt()

    override fun toString(): String = "EntityId($index#$generation)"

    public companion object {
        public fun of(
            index: Int,
            generation: Int,
        ): EntityId = EntityId((generation.toLong() shl 32) or (index.toLong() and 0xFFFFFFFFL))
    }
}
