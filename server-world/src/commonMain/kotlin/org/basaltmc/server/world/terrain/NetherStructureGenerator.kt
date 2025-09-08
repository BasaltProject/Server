package org.basaltmc.server.world.terrain

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.densityfunction.DensityFunction
import org.basaltmc.server.world.densityfunction.DensityFunctions
import org.basaltmc.server.world.structure.JigsawAssembly
import org.basaltmc.server.world.structure.PlacedStructurePiece
import org.basaltmc.server.world.structure.StructureBoundingBox
import org.basaltmc.server.world.structure.StructureMirror
import org.basaltmc.server.world.structure.StructurePlacementGrid
import org.basaltmc.server.world.structure.StructurePos
import org.basaltmc.server.world.structure.StructureRotation
import org.basaltmc.server.world.structure.StructureStartRecord
import org.basaltmc.server.world.structure.StructureTemplate
import org.basaltmc.server.world.structure.StructureTemplatePool
import org.basaltmc.server.world.structure.TemplateLoader
import org.basaltmc.wire.common.Identifier
import kotlin.concurrent.AtomicReference

/**
 * Places Nether structure starts before decoration. Fossils, weighted bastions, procedural
 * fortresses, and Nether ruined portals are wired from the 26.2 streams. Complete neighbor-derived
 * template states, unsupported block-entity kinds, structure entities, and scheduled-tick
 * execution remain pipeline gaps.
 */
public class NetherStructureGenerator(
    private val router: NetherNoiseRouter,
    private val biomeSource: NetherBiomeSource,
    private val templateLoader: TemplateLoader,
    poolLookup: (String) -> StructureTemplatePool? = { null },
) {
    private val samplerCache = AtomicReference<Map<Pair<Int, Int>, DensityChunkSampler>>(emptyMap())
    private val bastionCache = AtomicReference<Map<BastionCacheKey, List<PlacedStructurePiece>>>(emptyMap())
    private val fortressCache = AtomicReference<Map<FortressCacheKey, List<NetherFortressPieces.Piece>>>(emptyMap())
    private val bastionAssembly = JigsawAssembly(poolLookup, templateLoader, heightAt = { _, _ -> 0 })
    private val ruinedPortalGenerator =
        NetherRuinedPortalGenerator(biomeSource, templateLoader, ::baseStateAt, ::placeBastions)

    public fun generateInto(
        chunk: Chunk,
        seed: Long,
    ) {
        placeBastions(chunk, seed)
        ruinedPortalGenerator.generateInto(chunk, seed)
        placeNetherFossils(chunk, seed)
        placeFortresses(chunk, seed)
    }

    /** // parity: NoiseChunk adds referenced structure terrain adaptation before cell interpolation. */
    internal fun finalDensityForChunk(
        chunkX: Int,
        chunkZ: Int,
        seed: Long,
    ): DensityFunction {
        val pieces =
            referencedNetherFossils(chunkX, chunkZ, seed).map { reference ->
                StructureBeardifier.Rigid(reference.boundingBox)
            }
        return if (pieces.isEmpty()) {
            router.finalDensity
        } else {
            DensityFunctions.add(router.finalDensity, StructureBeardifier(pieces))
        }
    }

    private fun placeBastions(
        chunk: Chunk,
        seed: Long,
    ) {
        val cellX = chunk.chunkX.floorDiv(NETHER_COMPLEX_GRID.spacing)
        val cellZ = chunk.chunkZ.floorDiv(NETHER_COMPLEX_GRID.spacing)
        val clip = chunkClip(chunk)
        val placementRandom = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = placementRandom.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        placementRandom.setFeatureSeed(decorationSeed, BASTION_STRUCTURE_INDEX, SURFACE_STRUCTURES_STEP)
        val references = mutableListOf<BastionReference>()
        for (cellOffsetX in -1..1) {
            for (cellOffsetZ in -1..1) {
                val (candidateX, candidateZ) =
                    NETHER_COMPLEX_GRID.candidateChunk(
                        seed,
                        (cellX + cellOffsetX) * NETHER_COMPLEX_GRID.spacing,
                        (cellZ + cellOffsetZ) * NETHER_COMPLEX_GRID.spacing,
                    )
                if (candidateX !in chunk.chunkX - STRUCTURE_REFERENCE_RANGE..chunk.chunkX + STRUCTURE_REFERENCE_RANGE) {
                    continue
                }
                if (candidateZ !in chunk.chunkZ - STRUCTURE_REFERENCE_RANGE..chunk.chunkZ + STRUCTURE_REFERENCE_RANGE) {
                    continue
                }
                if (!selectsBastion(seed, candidateX, candidateZ)) continue
                val pieces = assembledBastion(seed, candidateX, candidateZ)
                if (pieces.isEmpty() || !pieces.boundingBox().intersects(clip)) continue
                references += BastionReference(candidateX, candidateZ, pieces)
            }
        }
        val referencesByChunk = references.associateBy { it.chunkX to it.chunkZ }
        val insertionOrder = references.sortedWith(compareBy(BastionReference::chunkX, BastionReference::chunkZ))
        for (position in vanillaStructureReferenceChunkOrder(insertionOrder.map { it.chunkX to it.chunkZ })) {
            val reference = referencesByChunk.getValue(position)
            for (piece in reference.pieces) {
                if (!piece.boundingBox.intersects(clip)) continue
                piece.element.place(templateLoader, chunk, piece.position, piece.rotation, clip, placementRandom)
            }
        }
    }

    private fun List<PlacedStructurePiece>.boundingBox(): StructureBoundingBox =
        drop(1).fold(first().boundingBox) { result, piece ->
            val box = piece.boundingBox
            StructureBoundingBox(
                minOf(result.minX, box.minX),
                minOf(result.minY, box.minY),
                minOf(result.minZ, box.minZ),
                maxOf(result.maxX, box.maxX),
                maxOf(result.maxY, box.maxY),
                maxOf(result.maxZ, box.maxZ),
            )
        }

    private fun selectsBastion(
        seed: Long,
        candidateChunkX: Int,
        candidateChunkZ: Int,
    ): Boolean {
        val random = WorldgenRandom(LegacyRandom(0L))
        random.setLargeFeatureSeed(seed, candidateChunkX, candidateChunkZ)
        return random.nextInt(NETHER_COMPLEX_TOTAL_WEIGHT) >= FORTRESS_WEIGHT
    }

    private fun assembledBastion(
        seed: Long,
        candidateChunkX: Int,
        candidateChunkZ: Int,
    ): List<PlacedStructurePiece> {
        val key = BastionCacheKey(seed, candidateChunkX, candidateChunkZ)
        bastionCache.value[key]?.let { return it }
        val random = WorldgenRandom(LegacyRandom(0L))
        random.setLargeFeatureSeed(seed, candidateChunkX, candidateChunkZ)
        val assembled =
            bastionAssembly.assemble(
                startPoolName = "minecraft:bastion/starts",
                startPosition = StructurePos(candidateChunkX * 16, BASTION_START_Y, candidateChunkZ * 16),
                maxDepth = 6,
                random = random,
                projectStartToHeightmap = false,
                maxDistanceFromCenter = 80,
                maxVerticalDistanceFromCenter = 80,
            )
        val valid =
            assembled.takeIf { pieces ->
                val startBox = pieces.firstOrNull()?.boundingBox ?: return@takeIf false
                val centerX = (startBox.minX + startBox.maxX) / 2
                val centerZ = (startBox.minZ + startBox.maxZ) / 2
                biomeSource.biomeAt(centerX, BASTION_START_Y, centerZ).id.path in BASTION_BIOMES
            } ?: emptyList()
        while (true) {
            val current = bastionCache.value
            current[key]?.let { return it }
            if (bastionCache.compareAndSet(current, current + (key to valid))) return valid
        }
    }

    private fun placeFortresses(
        chunk: Chunk,
        seed: Long,
    ) {
        val cellX = chunk.chunkX.floorDiv(NETHER_COMPLEX_GRID.spacing)
        val cellZ = chunk.chunkZ.floorDiv(NETHER_COMPLEX_GRID.spacing)
        val clip = chunkClip(chunk)
        val placementRandom = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = placementRandom.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        placementRandom.setFeatureSeed(decorationSeed, FORTRESS_STRUCTURE_INDEX, UNDERGROUND_DECORATION_STEP)
        val references = mutableListOf<FortressReference>()
        for (cellOffsetX in -1..1) {
            for (cellOffsetZ in -1..1) {
                val (candidateX, candidateZ) =
                    NETHER_COMPLEX_GRID.candidateChunk(
                        seed,
                        (cellX + cellOffsetX) * NETHER_COMPLEX_GRID.spacing,
                        (cellZ + cellOffsetZ) * NETHER_COMPLEX_GRID.spacing,
                    )
                if (candidateX !in chunk.chunkX - STRUCTURE_REFERENCE_RANGE..chunk.chunkX + STRUCTURE_REFERENCE_RANGE) {
                    continue
                }
                if (candidateZ !in chunk.chunkZ - STRUCTURE_REFERENCE_RANGE..chunk.chunkZ + STRUCTURE_REFERENCE_RANGE) {
                    continue
                }
                if (!selectsFortress(seed, candidateX, candidateZ)) continue
                val pieces = assembledFortress(seed, candidateX, candidateZ)
                if (pieces.isEmpty() || !pieces.fortressBoundingBox().intersects(clip)) continue
                references += FortressReference(candidateX, candidateZ, pieces)
            }
        }
        for (reference in references) recordFortress(chunk, clip, reference)
        val referencesByChunk = references.associateBy { it.chunkX to it.chunkZ }
        val insertionOrder = references.sortedWith(compareBy(FortressReference::chunkX, FortressReference::chunkZ))
        for (position in vanillaStructureReferenceChunkOrder(insertionOrder.map { it.chunkX to it.chunkZ })) {
            val reference = referencesByChunk.getValue(position)
            for (piece in reference.pieces) {
                if (piece.boundingBox.intersects(clip)) piece.place(chunk, clip, placementRandom)
            }
        }
    }

    /**
     * Records the slice of a fortress that reaches [chunk]. The monster override tests piece
     * bounds, so only the pieces this chunk holds are kept; the full bounds serve the separate
     * nether-brick gate, which tests the fortress as a whole.
     */
    private fun recordFortress(
        chunk: Chunk,
        clip: StructureBoundingBox,
        reference: FortressReference,
    ) {
        val here = reference.pieces.filter { it.boundingBox.intersects(clip) }
        if (here.isEmpty()) return
        chunk.structures.add(
            StructureStartRecord(
                FORTRESS_NAME,
                reference.chunkX,
                reference.chunkZ,
                reference.pieces.fortressBoundingBox(),
                here.map { it.boundingBox },
            ),
        )
    }

    private fun selectsFortress(
        seed: Long,
        candidateChunkX: Int,
        candidateChunkZ: Int,
    ): Boolean {
        val random = WorldgenRandom(LegacyRandom(0L))
        random.setLargeFeatureSeed(seed, candidateChunkX, candidateChunkZ)
        return random.nextInt(NETHER_COMPLEX_TOTAL_WEIGHT) < FORTRESS_WEIGHT
    }

    private fun assembledFortress(
        seed: Long,
        candidateChunkX: Int,
        candidateChunkZ: Int,
    ): List<NetherFortressPieces.Piece> {
        val key = FortressCacheKey(seed, candidateChunkX, candidateChunkZ)
        fortressCache.value[key]?.let { return it }
        val random = WorldgenRandom(LegacyRandom(0L))
        random.setLargeFeatureSeed(seed, candidateChunkX, candidateChunkZ)
        val assembled =
            if (biomeSource.biomeAt(candidateChunkX * 16, FORTRESS_START_Y, candidateChunkZ * 16).id.path in
                NETHER_BIOMES
            ) {
                NetherFortressPieces.assemble(random, candidateChunkX, candidateChunkZ)
            } else {
                emptyList()
            }
        while (true) {
            val current = fortressCache.value
            current[key]?.let { return it }
            if (fortressCache.compareAndSet(current, current + (key to assembled))) return assembled
        }
    }

    private fun List<NetherFortressPieces.Piece>.fortressBoundingBox(): StructureBoundingBox =
        drop(1).fold(first().boundingBox) { result, piece ->
            val box = piece.boundingBox
            StructureBoundingBox(
                minOf(result.minX, box.minX),
                minOf(result.minY, box.minY),
                minOf(result.minZ, box.minZ),
                maxOf(result.maxX, box.maxX),
                maxOf(result.maxY, box.maxY),
                maxOf(result.maxZ, box.maxZ),
            )
        }

    private fun placeNetherFossils(
        chunk: Chunk,
        seed: Long,
    ) {
        val clip = chunkClip(chunk)
        for (reference in referencedNetherFossils(chunk.chunkX, chunk.chunkZ, seed)) {
            reference.template.placeInto(
                chunk,
                reference.start.position,
                reference.start.rotation,
                StructureMirror.NONE,
                ZERO_POS,
                clip,
                ignoreStructureBlocks = true,
                ignoreAir = true,
            )
            placeDriedGhast(chunk, seed, reference.boundingBox, clip)
        }
    }

    private fun referencedNetherFossils(
        chunkX: Int,
        chunkZ: Int,
        seed: Long,
    ): List<FossilReference> {
        val cellX = chunkX.floorDiv(NETHER_FOSSIL_GRID.spacing)
        val cellZ = chunkZ.floorDiv(NETHER_FOSSIL_GRID.spacing)
        val clip = chunkColumnClip(chunkX, chunkZ)
        val references = mutableListOf<FossilReference>()
        for (cellOffsetX in -FOSSIL_SEARCH_CELLS..FOSSIL_SEARCH_CELLS) {
            for (cellOffsetZ in -FOSSIL_SEARCH_CELLS..FOSSIL_SEARCH_CELLS) {
                val (candidateX, candidateZ) =
                    NETHER_FOSSIL_GRID.candidateChunk(
                        seed,
                        (cellX + cellOffsetX) * NETHER_FOSSIL_GRID.spacing,
                        (cellZ + cellOffsetZ) * NETHER_FOSSIL_GRID.spacing,
                    )
                val start = netherFossilStart(candidateX, candidateZ, seed) ?: continue
                val template = templateLoader(start.template) ?: continue
                val boundingBox =
                    template.getBoundingBox(
                        start.position,
                        start.rotation,
                        ZERO_POS,
                        StructureMirror.NONE,
                    )
                if (!boundingBox.intersects(clip)) continue
                references += FossilReference(candidateX, candidateZ, start, template, boundingBox)
            }
        }
        val referencesByChunk = references.associateBy { it.chunkX to it.chunkZ }
        val insertionOrder = references.sortedWith(compareBy(FossilReference::chunkX, FossilReference::chunkZ))
        return vanillaStructureReferenceChunkOrder(insertionOrder.map { it.chunkX to it.chunkZ }).map {
            referencesByChunk.getValue(it)
        }
    }

    private fun netherFossilStart(
        candidateChunkX: Int,
        candidateChunkZ: Int,
        seed: Long,
    ): FossilStart? {
        val random = WorldgenRandom(LegacyRandom(0L))
        random.setLargeFeatureSeed(seed, candidateChunkX, candidateChunkZ)
        val x = candidateChunkX * 16 + random.nextInt(16)
        val z = candidateChunkZ * 16 + random.nextInt(16)
        var y = FOSSIL_MIN_Y + random.nextInt(FOSSIL_MAX_Y - FOSSIL_MIN_Y + 1)
        var current = baseStateAt(x, y, z)
        while (y > NETHER_SEA_LEVEL) {
            val below = baseStateAt(x, --y, z)
            if (Blocks.isAir(current) && below == Blocks.NETHERRACK.stateId) break
            current = below
        }
        if (y <= NETHER_SEA_LEVEL) return null
        if (biomeSource.biomeAt(x, y, z).id.path != "soul_sand_valley") return null
        val rotation = StructureRotation.entries[random.nextInt(StructureRotation.entries.size)]
        val template = FOSSIL_TEMPLATES[random.nextInt(FOSSIL_TEMPLATES.size)]
        return FossilStart(StructurePos(x, y, z), rotation, template)
    }

    private fun baseStateAt(
        x: Int,
        y: Int,
        z: Int,
    ): Int {
        if (samplerAt(x, z).sample(x and 15, y, z and 15) > 0.0) {
            return Blocks.NETHERRACK.stateId
        }
        return if (y < NETHER_SEA_LEVEL) Blocks.LAVA.stateId else Blocks.AIR.stateId
    }

    private fun samplerAt(
        x: Int,
        z: Int,
    ): DensityChunkSampler {
        val key = (x shr 4) to (z shr 4)
        samplerCache.value[key]?.let { return it }
        val created = DensityChunkSampler(router.finalDensity, key.first, key.second, 0, NETHER_GENERATION_DEPTH, 4, 8)
        while (true) {
            val current = samplerCache.value
            current[key]?.let { return it }
            if (samplerCache.compareAndSet(current, current + (key to created))) return created
        }
    }

    private fun placeDriedGhast(
        chunk: Chunk,
        seed: Long,
        fossilBox: StructureBoundingBox,
        clip: StructureBoundingBox,
    ) {
        val center =
            StructurePos(
                fossilBox.minX + (fossilBox.maxX - fossilBox.minX + 1) / 2,
                fossilBox.minY + (fossilBox.maxY - fossilBox.minY + 1) / 2,
                fossilBox.minZ + (fossilBox.maxZ - fossilBox.minZ + 1) / 2,
            )
        val random = LegacyRandom(seed).forkPositional().at(center.x, center.y, center.z)
        if (random.nextFloat() >= 0.5f) return
        val position =
            StructurePos(
                fossilBox.minX + random.nextInt(fossilBox.maxX - fossilBox.minX + 1),
                fossilBox.minY,
                fossilBox.minZ + random.nextInt(fossilBox.maxZ - fossilBox.minZ + 1),
            )
        if (!clip.isInside(position)) return
        val current = chunk.getBlockStateId(position.x and 15, position.y, position.z and 15)
        if (!Blocks.isAir(current)) return
        random.nextInt(StructureRotation.entries.size)
        chunk.setBlockStateId(position.x and 15, position.y, position.z and 15, stateId("dried_ghast"))
    }

    private fun chunkClip(chunk: Chunk): StructureBoundingBox =
        StructureBoundingBox(
            chunk.chunkX * 16,
            chunk.verticalBounds.minY,
            chunk.chunkZ * 16,
            chunk.chunkX * 16 + 15,
            chunk.verticalBounds.maxY,
            chunk.chunkZ * 16 + 15,
        )

    private fun chunkColumnClip(
        chunkX: Int,
        chunkZ: Int,
    ): StructureBoundingBox =
        StructureBoundingBox(
            chunkX * 16,
            Int.MIN_VALUE,
            chunkZ * 16,
            chunkX * 16 + 15,
            Int.MAX_VALUE,
            chunkZ * 16 + 15,
        )

    private data class FossilStart(
        val position: StructurePos,
        val rotation: StructureRotation,
        val template: String,
    )

    private data class FossilReference(
        val chunkX: Int,
        val chunkZ: Int,
        val start: FossilStart,
        val template: StructureTemplate,
        val boundingBox: StructureBoundingBox,
    )

    private data class BastionCacheKey(
        val seed: Long,
        val chunkX: Int,
        val chunkZ: Int,
    )

    private data class FortressCacheKey(
        val seed: Long,
        val chunkX: Int,
        val chunkZ: Int,
    )

    private data class BastionReference(
        val chunkX: Int,
        val chunkZ: Int,
        val pieces: List<PlacedStructurePiece>,
    )

    private data class FortressReference(
        val chunkX: Int,
        val chunkZ: Int,
        val pieces: List<NetherFortressPieces.Piece>,
    )

    internal companion object {
        const val NETHER_SEA_LEVEL = 32
        const val NETHER_GENERATION_DEPTH = 128
        const val FOSSIL_MIN_Y = 32
        const val FOSSIL_MAX_Y = 125
        const val FOSSIL_SEARCH_CELLS = 2
        const val BASTION_START_Y = 33
        const val FORTRESS_START_Y = 64
        const val FORTRESS_WEIGHT = 2
        const val NETHER_COMPLEX_TOTAL_WEIGHT = 5
        const val FORTRESS_NAME = "fortress"
        const val FORTRESS_STRUCTURE_INDEX = 1
        const val BASTION_STRUCTURE_INDEX = 0
        const val SURFACE_STRUCTURES_STEP = 4
        const val UNDERGROUND_DECORATION_STEP = 7
        const val STRUCTURE_REFERENCE_RANGE = 8
        val ZERO_POS = StructurePos(0, 0, 0)
        val NETHER_FOSSIL_GRID = StructurePlacementGrid(spacing = 2, separation = 1, salt = 14357921)
        val NETHER_COMPLEX_GRID = StructurePlacementGrid(spacing = 27, separation = 4, salt = 30084232)
        val FOSSIL_TEMPLATES = (1..14).map { "minecraft:nether_fossils/fossil_$it" }
        val BASTION_BIOMES = setOf("crimson_forest", "nether_wastes", "soul_sand_valley", "warped_forest")
        val NETHER_BIOMES = BASTION_BIOMES + "basalt_deltas"

        fun stateId(path: String): Int =
            Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId
                ?: error("missing vanilla block minecraft:$path")
    }
}

/**
 * // parity: ChunkAccess creates a default fastutil LongOpenHashSet for structure references;
 * its iterator returns the zero key first and then occupied slots from 31 down to 0. Inputs must
 * be in ChunkGenerator.createReferences insertion order (source X, then source Z).
 */
internal fun vanillaStructureReferenceChunkOrder(references: List<Pair<Int, Int>>): List<Pair<Int, Int>> {
    require(references.size <= 23) { "default vanilla reference set would rehash" }
    val table = arrayOfNulls<Pair<Int, Int>>(32)
    var zeroReference: Pair<Int, Int>? = null
    for (reference in references) {
        val key =
            (reference.first.toLong() and 0xffffffffL) or
                ((reference.second.toLong() and 0xffffffffL) shl 32)
        if (key == 0L) {
            zeroReference = reference
            continue
        }
        var mixed = key * -7046029254386353131L
        mixed = mixed xor (mixed ushr 32)
        mixed = mixed xor (mixed ushr 16)
        var slot = mixed.toInt() and 31
        while (table[slot] != null) slot = (slot + 1) and 31
        table[slot] = reference
    }
    return buildList {
        zeroReference?.let(::add)
        for (slot in 31 downTo 0) table[slot]?.let(::add)
    }
}
