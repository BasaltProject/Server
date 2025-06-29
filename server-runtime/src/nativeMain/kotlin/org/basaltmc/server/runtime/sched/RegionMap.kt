package org.basaltmc.server.runtime.sched

/**
 * How the loaded chunks of a world are grouped into regions.
 *
 * parity of model with `ThreadedRegionizer`: the world is cut into a grid of sections, and a region
 * is a set of sections that touch each other. Two chunks that can interact inside one tick have to
 * end up in the same region, which is why the grid is coarse (sixteen chunks a side, Folia's own
 * `gridExponent = 4`) and why sections that touch diagonally count as connected.
 *
 * Pure on purpose: it takes the sections that have chunks in them and answers the groups. Who owns
 * what, what to do when two groups become one and what happens to the work already queued are
 * [RegionScheduler]'s, and are the part that has to run at a tick boundary.
 */
public object RegionMap {
    /** parity: Folia's `gridExponent`, which makes a section sixteen chunks a side. */
    public const val GRID_EXPONENT: Int = 4

    /** The section a chunk belongs to. */
    public fun sectionOf(chunkCoordinate: Int): Int = chunkCoordinate shr GRID_EXPONENT

    /** One section as a single number, which is how the sets below are keyed. */
    public fun sectionKey(
        sectionX: Int,
        sectionZ: Int,
    ): Long = (sectionX.toLong() shl 32) or (sectionZ.toLong() and 0xFFFFFFFFL)

    public fun sectionX(key: Long): Int = (key shr 32).toInt()

    public fun sectionZ(key: Long): Int = key.toInt()

    /** The section key of a chunk, which is what a caller usually has. */
    public fun sectionKeyOfChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Long = sectionKey(sectionOf(chunkX), sectionOf(chunkZ))

    /**
     * The groups of sections that touch each other, largest first so the biggest group keeps the
     * region that was already there when several of them merge.
     */
    public fun components(sections: Set<Long>): List<Set<Long>> {
        if (sections.isEmpty()) return emptyList()
        val remaining = HashSet(sections)
        val groups = mutableListOf<Set<Long>>()
        val pending = ArrayDeque<Long>()
        while (remaining.isNotEmpty()) {
            val start = remaining.first()
            remaining.remove(start)
            val group = HashSet<Long>()
            group.add(start)
            pending.addLast(start)
            while (pending.isNotEmpty()) {
                val current = pending.removeFirst()
                val x = sectionX(current)
                val z = sectionZ(current)
                for (offsetX in -1..1) {
                    for (offsetZ in -1..1) {
                        if (offsetX == 0 && offsetZ == 0) continue
                        val neighbour = sectionKey(x + offsetX, z + offsetZ)
                        if (!remaining.remove(neighbour)) continue
                        group.add(neighbour)
                        pending.addLast(neighbour)
                    }
                }
            }
            groups.add(group)
        }
        // Biggest first: when two groups become one, the survivor should be the one that already
        // owns most of the map, so the fewest tasks and entities have to change hands.
        return groups.sortedByDescending { it.size }
    }
}
