package org.basaltmc.world.terrain

import org.basaltmc.random.LegacyRandom
import org.basaltmc.random.WorldgenRandom
import org.basaltmc.world.Chunk
import org.basaltmc.world.WorldHeight
import org.basaltmc.world.densityfunction.FunctionContext
import org.basaltmc.world.structure.JigsawAssembly
import org.basaltmc.world.structure.StructureBoundingBox
import org.basaltmc.world.structure.StructurePlacementGrid
import org.basaltmc.world.structure.StructurePos
import org.basaltmc.world.structure.StructureTemplatePool
import org.basaltmc.world.structure.TemplateLoader
import kotlin.math.abs

/**
 * Places real Plains village structures during chunk generation. // parity, narrowed:
 * `ChunkGenerator.createStructure`/`WorldGenRegion`'s own structure-starts-then-references
 * pipeline caches a structure's assembled pieces once (in the placement chunk's own saved data)
 * and has every overlapping chunk *reference* that cache when placing blocks. This project's
 * chunks generate statelessly with no such shared cache, so instead: [generateInto] recomputes
 * the entire deterministic [JigsawAssembly] from scratch for every nearby candidate placement
 * chunk (same seed, same grid cell, same result every time), then keeps only the pieces landing
 * inside [chunkX]/[chunkZ]. More CPU than vanilla's cache, but stateless and correct for any
 * chunk generation order.
 *
 * The assembly's own random is **not** vanilla's real per-structure seed (that formula was not
 * decompiled for this pass): reseeded with the same `setLargeFeatureWithSalt` formula the
 * placement grid itself uses, over a fresh [LegacyRandom] source, a documented gap.
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

                val startPosition = StructurePos(candidateChunkX * 16, 0, candidateChunkZ * 16)
                val random = WorldgenRandom(LegacyRandom(0L))
                random.setLargeFeatureWithSalt(seed, candidateChunkX, candidateChunkZ, VILLAGE_ASSEMBLY_SALT)

                val pieces =
                    assembly.assemble(
                        startPoolName,
                        startPosition,
                        maxDepth,
                        random,
                        projectStartToHeightmap = true,
                    )
                for (piece in pieces) {
                    if (!piece.boundingBox.intersects(clip)) continue
                    piece.element.place(templateLoader, chunk, piece.position, piece.rotation, clip, random)
                }
            }
        }
    }

    private fun surfaceHeightAt(
        x: Int,
        z: Int,
    ): Int {
        var y = WorldHeight.MAX_Y
        while (y >= WorldHeight.MIN_Y &&
            router.finalDensity.compute(FunctionContext(x, y, z)) <= 0.0
        ) {
            y--
        }
        return y + 1
    }

    private companion object {
        const val VILLAGE_ASSEMBLY_SALT = 0
    }
}
