package org.basaltmc.server.world.terrain

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.densityfunction.DensityFunction
import org.basaltmc.server.world.densityfunction.DensityFunctions
import org.basaltmc.server.world.structure.JigsawAssembly
import org.basaltmc.server.world.structure.PlacedStructurePiece
import org.basaltmc.server.world.structure.StructureBoundingBox
import org.basaltmc.server.world.structure.StructurePlacementGrid
import org.basaltmc.server.world.structure.StructurePos
import org.basaltmc.server.world.structure.StructureProjection
import org.basaltmc.server.world.structure.StructureTemplatePool
import org.basaltmc.server.world.structure.TemplateLoader
import org.basaltmc.wire.common.Identifier
import kotlin.concurrent.AtomicReference
import kotlin.math.abs

/** Where a village was found, and which of the five it is. */
public data class LocatedVillage(
    public val structure: Identifier,
    public val chunkX: Int,
    public val chunkZ: Int,
)

/**
 * Places villages during chunk generation, choosing the variant from the biome at each candidate
 * position like vanilla's shared `villages` structure set (see [selectVariant]).
 *
 * // parity, narrowed: vanilla caches the assembled pieces in the placement chunk's saved data.
 * Chunks here are stateless, so [generateInto] recomputes the [JigsawAssembly] per nearby candidate
 * and keeps what lands here; [assemblyCache] makes that affordable, one measuring ~735ms.
 *
 * Starts are rebuilt with vanilla's independent Legacy structure seed. In step four each variant
 * owns its dynamic-registry index, and every start and piece draws one shared xoroshiro stream.
 */

public class VillageStructureGenerator(
    private val router: OverworldNoiseRouter,
    private val grid: StructurePlacementGrid,
    private val biomeAt: (x: Int, y: Int, z: Int) -> String,
    private val maxDepth: Int,
    private val maxDistanceFromCenter: Int,
    private val poolLookup: (String) -> StructureTemplatePool?,
    private val templateLoader: TemplateLoader,
) {
    // Bounded: it used to grow with every candidate ever visited, which on a long-running server is
    // a leak. Overflow drops the whole map rather than picking victims, because a CAS-swapped
    // immutable map carries no recency order; the ~9 candidates near the player are re-derived once
    // and the rest was dead weight anyway.
    private val assemblyCache = AtomicReference<Map<CandidateKey, VillageStart?>>(emptyMap())

    /** Cached candidate count and their total placed pieces, for the timings memory breakdown. */
    public fun assemblyStats(): Pair<Int, Long> {
        val cache = assemblyCache.value
        var pieces = 0L
        for (start in cache.values) pieces += start?.pieces?.size ?: 0
        return cache.size to pieces
    }

    public fun generateInto(
        chunk: Chunk,
        chunkX: Int,
        chunkZ: Int,
        seed: Long,
    ) {
        val clip =
            StructureBoundingBox(
                chunkX * 16,
                WorldHeight.MIN_Y,
                chunkZ * 16,
                chunkX * 16 + 15,
                WorldHeight.MAX_Y,
                chunkZ * 16 + 15,
            )

        val starts = referencedStarts(chunkX, chunkZ, seed, clip)
        val decorationRandom = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = decorationRandom.setDecorationSeed(seed, chunkX * 16, chunkZ * 16)
        for (variant in PLACEMENT_VARIANTS) {
            decorationRandom.setFeatureSeed(decorationSeed, variant.structureIndex, SURFACE_STRUCTURES_STEP)
            for (start in starts) {
                if (start.variant !== variant) continue
                for (piece in start.pieces) {
                    if (!piece.boundingBox.intersects(clip)) continue
                    piece.element.place(templateLoader, chunk, piece.position, piece.rotation, clip, decorationRandom)
                }
            }
        }
    }

    /** Every village structure this generator can make, by the id `/locate` and `/place` name. */
    public val structureIds: Set<Identifier> get() = STRUCTURE_IDS

    /**
     * The nearest chunk where [structure] would generate, searched outward over the placement grid.
     *
     * parity: `ChunkGenerator.findNearestMapStructure`, which walks the structure set's own cells
     * ring by ring rather than scanning chunks, and tests each candidate exactly as generation
     * would: the same weighted draw against the biome at the candidate's own centre column. A
     * candidate that draws a different village is not a match, which is why this cannot be answered
     * from the grid alone.
     */
    public fun locate(
        seed: Long,
        fromChunkX: Int,
        fromChunkZ: Int,
        structure: Identifier?,
        maxCells: Int = DEFAULT_SEARCH_CELLS,
    ): LocatedVillage? {
        val cellX = fromChunkX.floorDiv(grid.spacing)
        val cellZ = fromChunkZ.floorDiv(grid.spacing)
        for (ring in 0..maxCells) {
            for (dx in -ring..ring) {
                for (dz in -ring..ring) {
                    // Only the ring itself: the inside was searched by a previous, smaller one.
                    if (ring != 0 && abs(dx) != ring && abs(dz) != ring) continue
                    val (candidateX, candidateZ) =
                        grid.candidateChunk(seed, (cellX + dx) * grid.spacing, (cellZ + dz) * grid.spacing)
                    val variant = selectVariant(candidateX, candidateZ, seed, HashMap()) ?: continue
                    if (structure != null && variant.id != structure) continue
                    return LocatedVillage(variant.id, candidateX, candidateZ)
                }
            }
        }
        return null
    }

    /**
     * Assembles [structure] at a chunk that is not its candidate, which is what `/place structure`
     * does: vanilla generates the structure where it was told, skipping the placement grid and the
     * biome check that decide where one appears on its own.
     */
    public fun assembleAt(
        structure: Identifier,
        chunkX: Int,
        chunkZ: Int,
        seed: Long,
    ): List<PlacedStructurePiece>? {
        val variant = VARIANTS.firstOrNull { it.id == structure } ?: return null
        val random = WorldgenRandom(LegacyRandom(0L))
        random.setLargeFeatureSeed(seed, chunkX, chunkZ)
        val samplers = HashMap<Long, DensityChunkSampler>()
        return JigsawAssembly(poolLookup, templateLoader, heightAt = { x, z -> surfaceHeightAt(x, z, samplers) })
            .assemble(
                variant.startPool,
                StructurePos(chunkX * 16, 0, chunkZ * 16),
                maxDepth,
                random,
                projectStartToHeightmap = true,
                maxDistanceFromCenter = maxDistanceFromCenter,
                useExpansionHack = true,
            )
    }

    /**
     * Assembles any template pool by name, which is what `/place jigsaw` does: no structure, no
     * placement grid and no biome, just a start pool and a depth.
     *
     * parity: `PlaceCommand.placeJigsaw`, minus its `target` argument. Vanilla uses it to pick which
     * jigsaw block of the start piece the assembly grows from; this assembly grows from every
     * jigsaw of the start piece, so naming one narrows nothing and the argument is accepted and
     * ignored rather than pretended.
     */
    public fun assemblePool(
        pool: String,
        chunkX: Int,
        chunkZ: Int,
        maxDepth: Int,
        seed: Long,
    ): List<PlacedStructurePiece> {
        val random = WorldgenRandom(LegacyRandom(0L))
        random.setLargeFeatureSeed(seed, chunkX, chunkZ)
        val samplers = HashMap<Long, DensityChunkSampler>()
        return JigsawAssembly(poolLookup, templateLoader, heightAt = { x, z -> surfaceHeightAt(x, z, samplers) })
            .assemble(
                pool,
                StructurePos(chunkX * 16, 0, chunkZ * 16),
                maxDepth,
                random,
                projectStartToHeightmap = true,
                maxDistanceFromCenter = maxDistanceFromCenter,
                useExpansionHack = true,
            )
    }

    /** Whether [pool] is a template pool the operator's own jar actually ships. */
    public fun hasPool(pool: String): Boolean = poolLookup(pool) != null

    /** Writes the part of [pieces] that lands in [chunk]. The caller repeats it per chunk they cover. */
    public fun placeInto(
        chunk: Chunk,
        pieces: List<PlacedStructurePiece>,
        seed: Long,
    ) {
        val clip =
            StructureBoundingBox(
                chunk.chunkX * 16,
                WorldHeight.MIN_Y,
                chunk.chunkZ * 16,
                chunk.chunkX * 16 + 15,
                WorldHeight.MAX_Y,
                chunk.chunkZ * 16 + 15,
            )
        val random = WorldgenRandom(XoroshiroRandom(seed))
        for (piece in pieces) {
            if (!piece.boundingBox.intersects(clip)) continue
            piece.element.place(templateLoader, chunk, piece.position, piece.rotation, clip, random)
        }
    }

    /** // parity: NoiseChunk adds Beardifier.forStructuresInChunk to router.finalDensity. */
    public fun finalDensityForChunk(
        chunkX: Int,
        chunkZ: Int,
        seed: Long,
    ): DensityFunction {
        val chunkMinX = chunkX * 16
        val chunkMinZ = chunkZ * 16
        val referenceClip =
            StructureBoundingBox(
                chunkMinX,
                WorldHeight.MIN_Y,
                chunkMinZ,
                chunkMinX + 15,
                WorldHeight.MAX_Y,
                chunkMinZ + 15,
            )
        val nearClip =
            StructureBoundingBox(
                chunkMinX - BEARD_RADIUS,
                WorldHeight.MIN_Y,
                chunkMinZ - BEARD_RADIUS,
                chunkMinX + 15 + BEARD_RADIUS,
                WorldHeight.MAX_Y,
                chunkMinZ + 15 + BEARD_RADIUS,
            )
        val rigids = mutableListOf<StructureBeardifier.Rigid>()
        val junctions = mutableListOf<StructureBeardifier.Junction>()
        for (start in referencedStarts(chunkX, chunkZ, seed, referenceClip)) {
            for (piece in start.pieces) {
                if (!piece.boundingBox.intersects(nearClip)) continue
                if (piece.element.projection == StructureProjection.RIGID) {
                    rigids += StructureBeardifier.Rigid(piece.boundingBox, piece.groundLevelDelta)
                }
                for (junction in piece.junctions) {
                    if (junction.sourceX > chunkMinX - BEARD_RADIUS &&
                        junction.sourceX < chunkMinX + 15 + BEARD_RADIUS &&
                        junction.sourceZ > chunkMinZ - BEARD_RADIUS &&
                        junction.sourceZ < chunkMinZ + 15 + BEARD_RADIUS
                    ) {
                        junctions +=
                            StructureBeardifier.Junction(
                                junction.sourceX,
                                junction.sourceGroundY,
                                junction.sourceZ,
                            )
                    }
                }
            }
        }
        return if (rigids.isEmpty() && junctions.isEmpty()) {
            router.finalDensity
        } else {
            DensityFunctions.add(router.finalDensity, StructureBeardifier(rigids, junctions))
        }
    }

    private fun referencedStarts(
        chunkX: Int,
        chunkZ: Int,
        seed: Long,
        clip: StructureBoundingBox,
    ): List<VillageStart> {
        val cellX = chunkX.floorDiv(grid.spacing)
        val cellZ = chunkZ.floorDiv(grid.spacing)
        val starts = mutableListOf<VillageStart>()
        for (dx in -1..1) {
            for (dz in -1..1) {
                val (candidateChunkX, candidateChunkZ) =
                    grid.candidateChunk(seed, (cellX + dx) * grid.spacing, (cellZ + dz) * grid.spacing)
                val reachChunks = (maxDistanceFromCenter + BEARD_RADIUS) / 16 + 1
                if (abs(candidateChunkX - chunkX) > reachChunks ||
                    abs(candidateChunkZ - chunkZ) > reachChunks
                ) {
                    continue
                }
                val start = assembledStart(candidateChunkX, candidateChunkZ, seed) ?: continue
                if (start.pieces.any { it.boundingBox.intersects(clip) }) starts += start
            }
        }
        return starts
    }

    /**
     * Chunks now generate concurrently (see [org.basaltmc.server.runtime.world.WorldState]'s own KDoc),
     * so more than one thread can reach the same never-before-assembled candidate at once. Rather
     * than a lock around [assembly] itself (it can take the better part of a second, and holding a
     * lock across it would recreate the exact global-serialization problem this whole pass exists
     * to fix), [assemblyCache] is a plain immutable map swapped in with compare-and-set: every
     * thread computes freely outside any lock, and a duplicate result from losing the race is
     * simply discarded in favor of whichever finished first (retried against the winner's map, not
     * overwriting it), the same tradeoff [org.basaltmc.server.runtime.world.WorldState] makes.
     */
    private fun assembledStart(
        candidateChunkX: Int,
        candidateChunkZ: Int,
        seed: Long,
    ): VillageStart? {
        val key = CandidateKey(seed, candidateChunkX, candidateChunkZ)
        if (assemblyCache.value.containsKey(key)) return assemblyCache.value[key]

        val startPosition = StructurePos(candidateChunkX * 16, 0, candidateChunkZ * 16)
        val random = WorldgenRandom(LegacyRandom(0L))
        random.setLargeFeatureSeed(seed, candidateChunkX, candidateChunkZ)
        // One sampler cache per assembly, alive exactly as long as the computation that needs it.
        // The Plains pools have 36 `terrain_matching` entries, each asking once per attempted
        // connection, so reuse *within* an assembly is what makes it finish; across assemblies
        // there is nothing to reuse, because a finished assembly is cached and never recomputed.
        // A shared bounded map here thrashed instead: overflowing mid-assembly threw away the very
        // samplers the assembly was still using, and one chunk measured 7.9 seconds.
        val samplers = HashMap<Long, DensityChunkSampler>()
        val variant = selectVariant(candidateChunkX, candidateChunkZ, seed, samplers)
        val assembled =
            variant?.let {
                VillageStart(
                    it,
                    JigsawAssembly(poolLookup, templateLoader, heightAt = { x, z -> surfaceHeightAt(x, z, samplers) })
                        .assemble(
                            it.startPool,
                            startPosition,
                            maxDepth,
                            random,
                            projectStartToHeightmap = true,
                            maxDistanceFromCenter = maxDistanceFromCenter,
                            useExpansionHack = true,
                        ),
                )
            }

        while (true) {
            val current = assemblyCache.value
            if (current.containsKey(key)) return current[key]
            val next =
                if (current.size >= ASSEMBLY_CACHE_CAPACITY) {
                    mapOf(key to assembled)
                } else {
                    current + (key to assembled)
                }
            if (assemblyCache.compareAndSet(current, next)) return assembled
        }
    }

    /**
     * // parity: ChunkGenerator's own structure-set generation loop (its real `else` branch for
     * multi-entry sets): the five real `village_*` structures share one `villages` structure set,
     * all weight 1, and are tried in a seeded weighted draw without replacement until one's
     * `has_structure/village_*` biome tag matches the biome at the candidate's own center column
     * (sampled at the density-derived surface height, the same spot the real heightmap-projected
     * start uses). No match anywhere means no village at that candidate at all, real behavior too.
     */
    private fun selectVariant(
        candidateChunkX: Int,
        candidateChunkZ: Int,
        seed: Long,
        samplers: HashMap<Long, DensityChunkSampler>,
    ): Variant? {
        val random = WorldgenRandom(LegacyRandom(0L))
        random.setLargeFeatureSeed(seed, candidateChunkX, candidateChunkZ)
        val centerX = candidateChunkX * 16 + 8
        val centerZ = candidateChunkZ * 16 + 8
        val biome = biomeAt(centerX, surfaceHeightAt(centerX, centerZ, samplers), centerZ).substringAfter(':')
        val options = VARIANTS.toMutableList()
        var total = options.size
        while (options.isNotEmpty()) {
            var choice = random.nextInt(total)
            var index = 0
            for (variant in options) {
                choice -= 1 // every real entry weighs 1
                if (choice < 0) break
                index++
            }
            val variant = options[index]
            if (biome in variant.biomes) return variant
            options.removeAt(index)
            total -= 1
        }
        return null
    }

    /**
     * Scans a [DensityChunkSampler] rather than calling `finalDensity.compute` without a chunk
     * cache. The sampler supplies exact marker-scoped interpolation and reuses every noise-cell
     * corner; the uncached graph made repeated jigsaw height queries prohibitively expensive. The
     * chunk's heightmap is not an option because the column is often outside the generated chunk.
     *
     * [samplers] belongs to the calling assembly (see [assembledStart]): one sampler per chunk
     * column touched, reused across the assembly's many `terrain_matching` queries and discarded
     * with the map when the assembly is done.
     */
    private fun surfaceHeightAt(
        x: Int,
        z: Int,
        samplers: HashMap<Long, DensityChunkSampler>,
    ): Int {
        val chunkX = x shr 4
        val chunkZ = z shr 4
        val key = (chunkX.toLong() and 0xFFFFFFFFL) or (chunkZ.toLong() shl 32)
        val sampler =
            samplers.getOrPut(key) {
                DensityChunkSampler(
                    router.finalDensity,
                    chunkX,
                    chunkZ,
                    WorldHeight.MIN_Y,
                    WorldHeight.HEIGHT,
                    CELL_WIDTH,
                    CELL_HEIGHT,
                )
            }
        val localX = x and 15
        val localZ = z and 15
        var y = WorldHeight.MAX_Y
        while (y >= WorldHeight.MIN_Y && sampler.sample(localX, y, localZ) <= 0.0) y--
        // parity: NoiseBasedChunkGenerator.getFirstFreeHeight samples the generated base column,
        // not density alone. The Overworld's global aquifer fills open columns through y=62, so
        // WORLD_SURFACE_WG is at least sea level even when the solid ocean floor is lower.
        return (y + 1).coerceAtLeast(SEA_LEVEL)
    }

    public companion object {
        /** Vanilla searches 100 cells for a village, which at spacing 34 is a long way out. */
        private const val DEFAULT_SEARCH_CELLS = 100

        private const val CELL_WIDTH = 4
        private const val CELL_HEIGHT = 8
        private const val SEA_LEVEL = 63
        private const val BEARD_RADIUS = 12
        private const val SURFACE_STRUCTURES_STEP = 4

        // 64 candidate cells is over 2000 chunks of travel at the villages grid spacing; a full
        // re-assembly after an overflow costs about a second per nearby candidate.
        private const val ASSEMBLY_CACHE_CAPACITY = 64

        private data class Variant(
            val id: Identifier,
            val startPool: String,
            val biomes: Set<String>,
            val structureIndex: Int,
        )

        private data class CandidateKey(
            val seed: Long,
            val chunkX: Int,
            val chunkZ: Int,
        )

        private data class VillageStart(
            val variant: Variant,
            val pieces: List<PlacedStructurePiece>,
        )

        // parity: villages.json's five entries in file order (all weight 1) with each structure's
        // own real `has_structure/village_*` biome tag contents.
        private val VARIANTS =
            listOf(
                Variant(
                    Identifier.vanilla("village_plains"),
                    "minecraft:village/plains/town_centers",
                    setOf("plains", "meadow"),
                    22,
                ),
                Variant(
                    Identifier.vanilla("village_desert"),
                    "minecraft:village/desert/town_centers",
                    setOf("desert"),
                    21,
                ),
                Variant(
                    Identifier.vanilla("village_savanna"),
                    "minecraft:village/savanna/town_centers",
                    setOf("savanna"),
                    23,
                ),
                Variant(
                    Identifier.vanilla("village_snowy"),
                    "minecraft:village/snowy/town_centers",
                    setOf("snowy_plains"),
                    24,
                ),
                Variant(
                    Identifier.vanilla("village_taiga"),
                    "minecraft:village/taiga/town_centers",
                    setOf("taiga"),
                    25,
                ),
            )

        private val PLACEMENT_VARIANTS = VARIANTS.sortedBy(Variant::structureIndex)

        /**
         * The same ids, without a generator in hand: what a command completes with in a world that
         * generates no villages at all, since the name is a fact about this build, not about it.
         */
        public val STRUCTURE_IDS: Set<Identifier> = VARIANTS.map { it.id }.toSet()
    }
}
