package org.basaltmc.server.world

/** Vertical section bounds carried by each chunk and its codecs. */
public data class ChunkHeight(
    public val minY: Int,
    public val height: Int,
) {
    init {
        require(minY % 16 == 0) { "minY must be section-aligned: $minY" }
        require(height > 0 && height % 16 == 0) { "height must be a positive section multiple: $height" }
        require(minSection in Byte.MIN_VALUE..Byte.MAX_VALUE) { "minimum section does not fit chunk NBT: $minSection" }
        require(maxSection in Byte.MIN_VALUE..Byte.MAX_VALUE) { "maximum section does not fit chunk NBT: $maxSection" }
    }

    public val maxY: Int get() = minY + height - 1
    public val minSection: Int get() = minY shr 4
    public val sectionCount: Int get() = height / 16
    public val maxSection: Int get() = minSection + sectionCount - 1

    public companion object {
        public val OVERWORLD: ChunkHeight = ChunkHeight(-64, 384)
        public val NETHER: ChunkHeight = ChunkHeight(0, 256)
        public val END: ChunkHeight = ChunkHeight(0, 256)
    }
}
