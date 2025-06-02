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
 * Places villages during chunk generation, choosing the variant from the biome at each candidate
 * position like vanilla's shared `villages` structure set (see [selectVariantStartPool]).
 *
 * // parity, narrowed: vanilla caches the assembled pieces in the placement chunk's saved data.
 * Chunks here are stateless, so [generateInto] recomputes the [JigsawAssembly] per nearby candidate
 * and keeps what lands in this one; [assemblyCache] makes that affordable, one measuring ~735ms.
 *
 * The randomness has two gaps: the assembly reseeds with `setLargeFeatureWithSalt` rather than
 * vanilla's per-structure seed, and placement reseeds per piece.
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
        val startPoolName = selectVariantStartPool(candidateChunkX, candidateChunkZ, seed) ?: return emptyList()
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
     * // parity: ChunkGenerator's own structure-set generation loop (its real `else` branch for
     * multi-entry sets): the five real `village_*` structures share one `villages` structure set,
     * all weight 1, and are tried in a seeded weighted draw without replacement until one's
     * `has_structure/village_*` biome tag matches the biome at the candidate's own center column
     * (sampled at the density-derived surface height, the same spot the real heightmap-projected
     * start uses). No match anywhere means no village at that candidate at all, real behavior too.
     */
    private fun selectVariantStartPool(
        candidateChunkX: Int,
        candidateChunkZ: Int,
        seed: Long,
    ): String? {
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
            if (biome in variant.biomes) return variant.startPool
            options.removeAt(index)
            total -= 1
        }
        return null
    }

    /**
     * Scans a [DensityChunkSampler] rather than calling `finalDensity.compute` per `y`, which is
     * the per-block evaluation [DensityChunkSampler]'s own KDoc rules out, and which hung the
     * generator here. The chunk's heightmap is not an option: the column is often outside the
     * chunk being generated.
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

        data class Variant(
            val startPool: String,
            val biomes: Set<String>,
        )

        // parity: villages.json's five entries in file order (all weight 1) with each structure's
        // own real `has_structure/village_*` biome tag contents.
        val VARIANTS =
            listOf(
                Variant("minecraft:village/plains/town_centers", setOf("plains", "meadow")),
                Variant("minecraft:village/desert/town_centers", setOf("desert")),
                Variant("minecraft:village/savanna/town_centers", setOf("savanna")),
                Variant("minecraft:village/snowy/town_centers", setOf("snowy_plains")),
                Variant("minecraft:village/taiga/town_centers", setOf("taiga")),
            )
    }
}
