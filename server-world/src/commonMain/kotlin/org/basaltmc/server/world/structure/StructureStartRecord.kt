package org.basaltmc.server.world.structure

/**
 * // parity: `StructureStart`, reduced to what a position query needs. A start lives in the chunk it
 * began in and every chunk it touches keeps a reference back to it, which is vanilla's split.
 */
public data class StructureStartRecord(
    /** The structure's registry path, without the namespace, as `fortress` or `ancient_city`. */
    val structure: String,
    /** The chunk the start belongs to, which is what a reference points at. */
    val chunkX: Int,
    val chunkZ: Int,
    /** // parity: `StructureStart.getBoundingBox`, the union of every piece. */
    val fullBounds: StructureBoundingBox,
    /** // parity: `StructureStart.getPieces`, each piece's own box. */
    val pieces: List<StructureBoundingBox>,
) {
    /** // parity: `StructureManager.structureHasPieceAt`. */
    public fun hasPieceAt(pos: StructurePos): Boolean = pieces.any { it.isInside(pos) }

    /** Whether [pos] falls in the start as a whole. // parity: `getStructureAt`. */
    public fun contains(pos: StructurePos): Boolean = fullBounds.isInside(pos)

    public companion object {
        /** Builds a start from its pieces, taking the full bounds as their union. */
        public fun of(
            structure: String,
            chunkX: Int,
            chunkZ: Int,
            pieces: List<StructureBoundingBox>,
        ): StructureStartRecord? {
            if (pieces.isEmpty()) return null
            var box = pieces[0]
            for (i in 1 until pieces.size) {
                val piece = pieces[i]
                box =
                    StructureBoundingBox(
                        minOf(box.minX, piece.minX),
                        minOf(box.minY, piece.minY),
                        minOf(box.minZ, piece.minZ),
                        maxOf(box.maxX, piece.maxX),
                        maxOf(box.maxY, piece.maxY),
                        maxOf(box.maxZ, piece.maxZ),
                    )
            }
            return StructureStartRecord(structure, chunkX, chunkZ, box, pieces)
        }
    }
}
