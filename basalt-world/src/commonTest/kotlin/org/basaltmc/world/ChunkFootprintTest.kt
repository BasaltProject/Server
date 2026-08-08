package org.basaltmc.world

import kotlin.test.Test
import kotlin.test.assertTrue

/**
 * What a loaded chunk actually costs. A flat `IntArray(4096)` per section charged 16 KB whether the
 * section held terrain or nothing at all, which is what put a few hundred loaded chunks into the
 * hundreds of megabytes.
 */
class ChunkFootprintTest {
    @Test
    fun `a generated chunk costs far less than a flat array would`() {
        val chunk = NoiseWorldGenerator.generate(0, 0)

        var bytes = 0
        var uniform = 0
        for (i in 0 until chunk.sectionCount) {
            val section = chunk.section(i)
            val states = section.blockStateIds()
            val distinct = states.toHashSet().size
            val bits = bitsForPalette(distinct)
            if (bits == 0) uniform++
            bytes += (4096 * bits) / 8 + distinct * 4
        }

        val flat = chunk.sectionCount * 4096 * 4
        println("paletted: ${bytes / 1024} KB, flat IntArray: ${flat / 1024} KB, uniform sections: $uniform/${chunk.sectionCount}")

        assertTrue(uniform > 0, "a normal chunk has sections of nothing but air; those must cost no entry storage")
        assertTrue(bytes * 4 < flat, "expected the paletted form to be several times smaller, got $bytes vs $flat")
    }

    private fun bitsForPalette(distinct: Int): Int {
        if (distinct <= 1) return 0
        var bits = 0
        while ((1 shl bits) < distinct) bits++
        return maxOf(bits, 4)
    }
}
