package org.basaltmc.world.terrain

import org.basaltmc.random.LegacyRandom
import org.basaltmc.random.WorldgenRandom
import org.basaltmc.world.Chunk
import org.basaltmc.world.WorldHeight
import org.basaltmc.world.structure.JigsawAssembly
import org.basaltmc.world.structure.PlacedStructurePiece
import org.basaltmc.world.structure.StructureBoundingBox
import org.basaltmc.world.structure.StructurePlacementGrid
import org.basaltmc.world.structure.StructurePos
import org.basaltmc.world.structure.StructureTemplatePool
import org.basaltmc.world.structure.TemplateLoader
import kotlin.concurrent.AtomicReference
import kotlin.math.abs

/**
 * Places real Plains village structures during chunk generation. // parity, narrowed:
 * `ChunkGenerator.createStructure`/`WorldGenRegion`'s own structure-starts-then-references
 * pipeline caches a structure's assembled pieces once (in the placement chunk's own saved data)
 * and has every overlapping chunk *reference* that cache when placing blocks. This project's
 * chunks generate statelessly with no such shared cache, so instead: [generateInto] recomputes
 * the deterministic [JigsawAssembly] for every nearby candidate placement chunk (same seed, same
 * grid cell, same result every time), then keeps only the pieces landing inside [chunkX]/[chunkZ].
 * [assemblyCache] avoids literally redoing that assembly for every one of the (typically dozens
 * of) nearby chunks a single village reaches: measured at ~735ms for one real Plains village's
 * ~45 pieces, an easy multi-minute blowup across a spawn-sized chunk sweep without it.
 *
 * The assembly's own random is **not** vanilla's real per-structure seed (that formula was not
 * decompiled for this pass): reseeded with the same `setLargeFeatureWithSalt` formula the
 * placement grid itself uses, over a fresh [LegacyRandom] source, a documented gap. Placement
 * (which still needs a random for processor rules like `mossify_20_percent`) uses its own
 * per-piece reseed rather than continuing the assembly random forward, since [assemblyCache]
 * means a cached assembly has no single "current" random state to continue from: real vanilla's
 * own placement draws are a continuation of the very same stream assembly used, so this is a
 * second, separate documented gap on top of the seed formula itself.
 */
public class VillageStructureGenerator(
    private val router: OverworldNoiseRouter,
    private val grid: StructurePlacementGrid,
    private val startPoolName: String,
    private val maxDepth: Int,
    private val maxDistanceFromCenter: Int,
    poolLookup: (String) -> StructureTemplatePool?,
    private val templateLoader: TemplateLoader,
) {
    private val assembly = JigsawAssembly(poolLookup, templateLoader, heightAt = { x, z -> surfaceHeightAt(x, z) })
    private val assemblyCache = AtomicReference<Map<Pair<Int, Int>, List<PlacedStructurePiece>>>(emptyMap())
    private val samplerCache = AtomicReference<Map<Pair<Int, Int>, DensityChunkSampler>>(emptyMap())

    public fun generateInto(
        chunk: Chunk,
        chunkX: Int,
        chunkZ: Int,
        seed: Long,
    ) {
        val cellX = chunkX.floorDiv(grid.spacing)
        val cellZ = chunkZ.floorDiv(grid.spacing)
        val clip =
            StructureBoundingBox(
                chunkX * 16,
                WorldHeight.MIN_Y,
                chunkZ * 16,
                chunkX * 16 + 15,
                WorldHeight.MAX_Y,
                chunkZ * 16 + 15,
            )

        for (dx in -1..1) {
            for (dz in -1..1) {
                val (candidateChunkX, candidateChunkZ) =
                    grid.candidateChunk(seed, (cellX + dx) * grid.spacing, (cellZ + dz) * grid.spacing)
                val reachChunks = (maxDistanceFromCenter / 16) + 1
                if (abs(candidateChunkX - chunkX) > reachChunks ||
                    abs(candidateChunkZ - chunkZ) > reachChunks
                ) {
                    continue
                }

                val pieces = assembledPieces(candidateChunkX, candidateChunkZ, seed)
                pieces.forEachIndexed { index, piece ->
                    if (!piece.boundingBox.intersects(clip)) return@forEachIndexed
                    val placeRandom = WorldgenRandom(LegacyRandom(0L))
                    placeRandom.setLargeFeatureWithSalt(
                        seed,
                        candidateChunkX * 31 + index,
                        candidateChunkZ,
                        VILLAGE_ASSEMBLY_SALT + 1,
                    )
                    piece.element.place(templateLoader, chunk, piece.position, piece.rotation, clip, placeRandom)
                }
            }
        }
    }

    /**
     * Chunks now generate concurrently (see [org.basaltmc.server.world.WorldState]'s own KDoc),
     * so more than one thread can reach the same never-before-assembled candidate at once. Rather
     * than a lock around [assembly] itself (it can take the better part of a second, and holding a
     * lock across it would recreate the exact global-serialization problem this whole pass exists
     * to fix), [assemblyCache] is a plain immutable map swapped in with compare-and-set: every
     * thread computes freely outside any lock, and a duplicate result from losing the race is
     * simply discarded in favor of whichever finished first (retried against the winner's map, not
     * overwriting it), the same tradeoff [org.basaltmc.server.world.WorldState] makes.
     */
    private fun assembledPieces(
        candidateChunkX: Int,
        candidateChunkZ: Int,
        seed: Long,
    ): List<PlacedStructurePiece> {
        val key = candidateChunkX to candidateChunkZ
        assemblyCache.value[key]?.let { return it }

        val startPosition = StructurePos(candidateChunkX * 16, 0, candidateChunkZ * 16)
        val random = WorldgenRandom(LegacyRandom(0L))
        random.setLargeFeatureWithSalt(seed, candidateChunkX, candidateChunkZ, VILLAGE_ASSEMBLY_SALT)
        val assembled =
            assembly.assemble(
                startPoolName,
                startPosition,
                maxDepth,
                random,
                projectStartToHeightmap = true,
                maxDistanceFromCenter = maxDistanceFromCenter,
            )

        while (true) {
            val current = assemblyCache.value
            current[key]?.let { return it }
            if (assemblyCache.compareAndSet(current, current + (key to assembled))) return assembled
        }
    }

    /**
     * **A real bug found and fixed after real gameplay reproduced a CPU-pegged hang**: this
     * originally called [OverworldNoiseRouter.finalDensity]`.compute` directly in a raw per-`y`
     * loop, exactly the "evaluate the whole density function tree per block" anti-pattern this
     * project's own [DensityChunkSampler] KDoc already documents as too slow to be viable (the
     * same mistake this project's surface-rule height estimation made once before and fixed by
     * switching to the chunk's own already-computed heightmap; this function can't reuse a
     * chunk's heightmap since the position it needs isn't always in the chunk currently
     * generating). A stress test reproduced this taking so long (worse yet, run from several
     * concurrent chunk-generation threads at once, per [org.basaltmc.server.world.WorldState]'s
     * own KDoc) that it looked indistinguishable from a true hang. Fixed the same way real vanilla
     * itself never evaluates density per-block: build a [DensityChunkSampler] for the column's own
     * chunk (real vanilla's own cell-interpolated grid, not an approximation, see that class's own
     * KDoc) and scan through it instead.
     *
     * That alone still wasn't enough: real Plains village pools turn out to have 36 real
     * `terrain_matching` entries (verified against every real `village/plains` pool json, not just
     * `town_centers`, which happens to be all `rigid`), each one calling this function once per
     * attempted connection during [JigsawAssembly.assemble]'s own search, not once per assembly.
     * [samplerCache] reuses one [DensityChunkSampler] (and so its one already-built interpolation
     * grid) across every call that lands in the same chunk, instead of rebuilding that grid from
     * scratch on every single call: the actual fix that got the reproduction stress test from
     * "never finishes" to completing cleanly.
     */
    private fun surfaceHeightAt(
        x: Int,
        z: Int,
    ): Int {
        val chunkX = x shr 4
        val chunkZ = z shr 4
        val key = chunkX to chunkZ
        val sampler = samplerCache.value[key] ?: createSampler(chunkX, chunkZ, key)
        val localX = x and 15
        val localZ = z and 15
        var y = WorldHeight.MAX_Y
        while (y >= WorldHeight.MIN_Y && sampler.sample(localX, y, localZ) <= 0.0) y--
        return y + 1
    }

    private fun createSampler(
        chunkX: Int,
        chunkZ: Int,
        key: Pair<Int, Int>,
    ): DensityChunkSampler {
        val created =
            DensityChunkSampler(
                router.finalDensity,
                chunkX,
                chunkZ,
                WorldHeight.MIN_Y,
                WorldHeight.HEIGHT,
                CELL_WIDTH,
                CELL_HEIGHT,
            )
        while (true) {
            val current = samplerCache.value
            current[key]?.let { return it }
            if (samplerCache.compareAndSet(current, current + (key to created))) return created
        }
    }

    private companion object {
        const val VILLAGE_ASSEMBLY_SALT = 0
        const val CELL_WIDTH = 4
        const val CELL_HEIGHT = 8
    }
}
