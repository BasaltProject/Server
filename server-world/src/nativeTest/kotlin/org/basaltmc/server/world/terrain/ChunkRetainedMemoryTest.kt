package org.basaltmc.server.world.terrain

import org.basaltmc.server.world.Chunk
import kotlin.native.runtime.GC
import kotlin.native.runtime.NativeRuntimeApi
import kotlin.test.Test
import kotlin.test.assertTrue

/**
 * Bounds what a generated chunk keeps resident. The assertion runs on [Chunk.residentBytes], which
 * is deterministic; the GC live-heap figures are printed as diagnostics only, because conservative
 * stack scanning and lazy sweeping make single-collection deltas jitter by tens of KB per chunk.
 * The second generation round is the part that would catch a real leak: shared state accumulating
 * per generated chunk grows there again, a one-time floor does not.
 */
@OptIn(NativeRuntimeApi::class, ExperimentalStdlibApi::class)
class ChunkRetainedMemoryTest {
    private fun liveHeapBytes(): Long {
        // Twice: one collection right after an allocation burst leaves ~45KB per fresh chunk
        // unswept, which reads as a leak that is not there. Same reason /timings collects twice.
        GC.collect()
        GC.collect()
        return GC.lastGCInfo
            ?.memoryUsageAfter
            ?.get("heap")
            ?.totalObjectsSizeBytes ?: 0L
    }

    @Test
    fun aGeneratedChunkRetainsTensOfKilobytes() {
        val world = SyntheticOverworld()
        // Shared lazies (surface rules, block tables, climate index) initialize outside the window.
        RealTerrainGenerator.generate(world.router, world.environment, -9, -9)

        val chunks = ArrayList<Chunk>(CHUNK_COUNT)
        val before = liveHeapBytes()
        for (i in 0 until CHUNK_COUNT) {
            // Synthetic climate occasionally lands on a biome with an unimplemented surface band;
            // those chunks are skipped rather than steering the layout around them.
            runCatching { RealTerrainGenerator.generate(world.router, world.environment, i % 8, i / 8) }
                .onSuccess(chunks::add)
        }
        val after = liveHeapBytes()
        println("GC-delta per generated chunk: ${(after - before) / chunks.size} bytes over ${chunks.size} chunks")

        val accounted = chunks.maxOf { it.residentBytes() }
        println("Accounted resident bytes, worst chunk: $accounted")

        val batch = chunks.size
        chunks.clear()
        val released = liveHeapBytes()
        println("Retained per generated chunk after discard: ${(released - before) / batch} bytes")

        // Generation must not accumulate shared state per chunk: a further round over the same
        // positions may keep the one-time floor above, but adding tens of KB per chunk again
        // means a real leak.
        //
        // The best of three rounds, not the first: this measures a whole heap, and the rest of the
        // suite running beside it puts its own garbage in the number. A real leak adds on every
        // round and survives the minimum; another test's allocation does not.
        var leakedPerChunk = Long.MAX_VALUE
        var previous = released
        repeat(3) {
            repeat(CHUNK_COUNT) {
                runCatching { RealTerrainGenerator.generate(world.router, world.environment, it % 8, it / 8) }
            }
            val round = liveHeapBytes()
            leakedPerChunk = minOf(leakedPerChunk, (round - previous) / CHUNK_COUNT)
            previous = round
        }
        println("Second round added per chunk: $leakedPerChunk bytes")
        assertTrue(
            leakedPerChunk < SECOND_ROUND_LEAK_CEILING_BYTES,
            "generation accumulates $leakedPerChunk bytes of shared state per chunk",
        )

        assertTrue(
            accounted < RETAINED_CEILING_BYTES,
            "a generated chunk keeps $accounted bytes of sections and heightmaps, over the ceiling",
        )
    }

    private companion object {
        const val CHUNK_COUNT = 32

        // Sections plus heightmaps of a fully solid chunk stay well under this when the palettes
        // are doing their job.
        const val RETAINED_CEILING_BYTES = 120_000L

        // Well above GC measurement jitter, well below any real per-chunk structure.
        const val SECOND_ROUND_LEAK_CEILING_BYTES = 16_000L
    }
}
