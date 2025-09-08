package org.basaltmc.server.world.structure

/**
 * The structure starts that reach into one chunk, sliced to the pieces that touch it: a spawn query
 * must not load a chunk or read one another region owns. The full bounds travel with every slice.
 */
public class ChunkStructures(
    private val onChange: () -> Unit = {},
) {
    private val backing = mutableListOf<StructureStartRecord>()

    public val isEmpty: Boolean get() = backing.isEmpty()

    public fun starts(): List<StructureStartRecord> = backing.toList()

    /** Whether any start of [structure] reaches this chunk, the cheap gate before a box test. */
    public fun has(structure: String): Boolean = backing.any { it.structure == structure }

    public fun add(start: StructureStartRecord) {
        // A chunk is generated once, but a start can be offered again by a neighbour's pass.
        if (backing.any { it.structure == start.structure && it.chunkX == start.chunkX && it.chunkZ == start.chunkZ }) {
            return
        }
        backing += start
        onChange()
    }

    /** Restores a decoded chunk without marking it dirty again. */
    public fun restore(starts: List<StructureStartRecord>) {
        backing.clear()
        backing += starts
    }
}
