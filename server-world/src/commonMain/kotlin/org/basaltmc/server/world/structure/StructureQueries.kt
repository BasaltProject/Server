package org.basaltmc.server.world.structure

/**
 * // parity: `StructureManager`, the position queries only. [chunkStructures] keeps this safe under
 * regional ownership: no query touches a chunk other than the one holding the position.
 */
public class StructureQueries(
    private val chunkStructures: (chunkX: Int, chunkZ: Int) -> ChunkStructures?,
) {
    /** // parity: `getStructureAt`, matching the start's full bounds. */
    public fun startAt(
        pos: StructurePos,
        structure: String,
    ): StructureStartRecord? = startsIn(pos).firstOrNull { it.structure == structure && it.contains(pos) }

    /** // parity: `getStructureWithPieceAt`, matching a piece's own bounds. */
    public fun startWithPieceAt(
        pos: StructurePos,
        structure: String,
    ): StructureStartRecord? = startsIn(pos).firstOrNull { it.structure == structure && it.hasPieceAt(pos) }

    /**
     * Every start reaching the position's chunk, in the order that chunk recorded them, which is
     * the order a query resolves ties in when a position belongs to two structures at once.
     */
    public fun startsIn(pos: StructurePos): List<StructureStartRecord> =
        chunkStructures(pos.x shr 4, pos.z shr 4)?.starts().orEmpty()
}
