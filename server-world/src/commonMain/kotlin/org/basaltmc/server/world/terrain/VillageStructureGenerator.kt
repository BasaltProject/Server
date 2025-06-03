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
import kotlin.concurrent.AtomicReference
import kotlin.math.abs

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
    poolLookup: (String) -> StructureTemplatePool?,
    private val templateLoader: TemplateLoader,
) {
    private val assembly = JigsawAssembly(poolLookup, templateLoader, heightAt = { x, z -> surfaceHeightAt(x, z) })
    private val assemblyCache = AtomicReference<Map<CandidateKey, VillageStart?>>(emptyMap())
    private val samplerCache = AtomicReference<Map<Pair<Int, Int>, DensityChunkSampler>>(emptyMap())

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
        val variant = selectVariant(candidateChunkX, candidateChunkZ, seed)
        val assembled =
            variant?.let {
                VillageStart(
                    it,
                    assembly.assemble(
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
            if (assemblyCache.compareAndSet(current, current + (key to assembled))) return assembled
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
    ): Variant? {
        val random = WorldgenRandom(LegacyRandom(0L))
        random.setLargeFeatureSeed(seed, candidateChunkX, candidateChunkZ)
        val centerX = candidateChunkX * 16 + 8
        val centerZ = candidateChunkZ * 16 + 8
        val biome = biomeAt(centerX, surfaceHeightAt(centerX, centerZ), centerZ).substringAfter(':')
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
     * [samplerCache] keeps one sampler per chunk across calls: the Plains pools have 36
     * `terrain_matching` entries, each asking once per attempted connection, not once per
     * assembly, so rebuilding the interpolation grid every time is what made it never finish.
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
        // parity: NoiseBasedChunkGenerator.getFirstFreeHeight samples the generated base column,
        // not density alone. The Overworld's global aquifer fills open columns through y=62, so
        // WORLD_SURFACE_WG is at least sea level even when the solid ocean floor is lower.
        return (y + 1).coerceAtLeast(SEA_LEVEL)
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
        const val CELL_WIDTH = 4
        const val CELL_HEIGHT = 8
        const val SEA_LEVEL = 63
        const val BEARD_RADIUS = 12
        const val SURFACE_STRUCTURES_STEP = 4

        data class Variant(
            val startPool: String,
            val biomes: Set<String>,
            val structureIndex: Int,
        )

        data class CandidateKey(
            val seed: Long,
            val chunkX: Int,
            val chunkZ: Int,
        )

        data class VillageStart(
            val variant: Variant,
            val pieces: List<PlacedStructurePiece>,
        )

        // parity: villages.json's five entries in file order (all weight 1) with each structure's
        // own real `has_structure/village_*` biome tag contents.
        val VARIANTS =
            listOf(
                Variant("minecraft:village/plains/town_centers", setOf("plains", "meadow"), 22),
                Variant("minecraft:village/desert/town_centers", setOf("desert"), 21),
                Variant("minecraft:village/savanna/town_centers", setOf("savanna"), 23),
                Variant("minecraft:village/snowy/town_centers", setOf("snowy_plains"), 24),
                Variant("minecraft:village/taiga/town_centers", setOf("taiga"), 25),
            )

        val PLACEMENT_VARIANTS = VARIANTS.sortedBy(Variant::structureIndex)
    }
}
