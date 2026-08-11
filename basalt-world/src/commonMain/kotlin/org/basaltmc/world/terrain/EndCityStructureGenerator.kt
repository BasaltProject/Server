package org.basaltmc.world.terrain

import org.basaltmc.common.Identifier
import org.basaltmc.nbt.NbtCompound
import org.basaltmc.random.LegacyRandom
import org.basaltmc.random.WorldgenRandom
import org.basaltmc.random.XoroshiroRandom
import org.basaltmc.world.Chunk
import org.basaltmc.world.ChunkGeneratedEntity
import org.basaltmc.world.structure.StructureBoundingBox
import org.basaltmc.world.structure.StructureMirror
import org.basaltmc.world.structure.StructurePlacementGrid
import org.basaltmc.world.structure.StructurePos
import org.basaltmc.world.structure.StructureRotation
import org.basaltmc.world.structure.TemplateLoader
import kotlin.concurrent.AtomicReference

/** Places exact procedural End City starts and their template pieces before End decoration. */
public class EndCityStructureGenerator(
    private val router: EndNoiseRouter,
    private val biomeSource: TheEndBiomeSource,
    private val templateLoader: TemplateLoader,
    private val surfaceHeightOverride: ((x: Int, z: Int) -> Int)? = null,
) {
    private val samplerCache = AtomicReference<Map<Pair<Int, Int>, DensityChunkSampler>>(emptyMap())
    private val cityCache = AtomicReference<Map<CityCacheKey, List<EndCityPieces.Piece>>>(emptyMap())

    public fun generateInto(
        chunk: Chunk,
        seed: Long,
    ) {
        val clip = chunkClip(chunk)
        val references = referencedCities(chunk.chunkX, chunk.chunkZ, seed, clip)
        if (references.isEmpty()) return

        val placementRandom = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = placementRandom.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        placementRandom.setFeatureSeed(decorationSeed, END_CITY_STRUCTURE_INDEX, SURFACE_STRUCTURES_STEP)
        for (reference in references) {
            for (piece in reference.pieces) {
                if (!piece.boundingBox.intersects(clip)) continue
                piece.template.placeInto(
                    chunk = chunk,
                    position = piece.position,
                    rotation = piece.rotation,
                    mirror = StructureMirror.NONE,
                    pivot = ZERO,
                    clipTo = clip,
                    random = placementRandom,
                    ignoreStructureBlocks = true,
                    ignoreAir = !piece.overwrite,
                )
                handleMarkers(chunk, piece, clip, placementRandom)
            }
        }
    }

    internal fun cityAt(
        seed: Long,
        candidateChunkX: Int,
        candidateChunkZ: Int,
    ): List<EndCityPieces.Piece> {
        val key = CityCacheKey(seed, candidateChunkX, candidateChunkZ)
        cityCache.value[key]?.let { return it }
        val random = WorldgenRandom(LegacyRandom(0L))
        random.setLargeFeatureSeed(seed, candidateChunkX, candidateChunkZ)
        val rotation = StructureRotation.entries[random.nextInt(StructureRotation.entries.size)]
        val startX = candidateChunkX * 16 + 7
        val startZ = candidateChunkZ * 16 + 7
        val (offsetX, offsetZ) = rotation.heightOffsets()
        val startY =
            minOf(
                surfaceHeightAt(startX, startZ),
                surfaceHeightAt(startX, startZ + offsetZ),
                surfaceHeightAt(startX + offsetX, startZ),
                surfaceHeightAt(startX + offsetX, startZ + offsetZ),
            )
        val biome = biomeSource.biomeAtQuart(startX shr 2, startY shr 2, startZ shr 2).id.path
        val pieces =
            if (startY >= MIN_START_Y && biome in END_CITY_BIOMES) {
                EndCityPieces.assemble(templateLoader, StructurePos(startX, startY, startZ), rotation, random)
            } else {
                emptyList()
            }
        while (true) {
            val current = cityCache.value
            current[key]?.let { return it }
            if (cityCache.compareAndSet(current, current + (key to pieces))) return pieces
        }
    }

    private fun referencedCities(
        chunkX: Int,
        chunkZ: Int,
        seed: Long,
        clip: StructureBoundingBox,
    ): List<CityReference> {
        val cellX = chunkX.floorDiv(END_CITY_GRID.spacing)
        val cellZ = chunkZ.floorDiv(END_CITY_GRID.spacing)
        val references = mutableListOf<CityReference>()
        for (cellOffsetX in -1..1) {
            for (cellOffsetZ in -1..1) {
                val (candidateX, candidateZ) =
                    END_CITY_GRID.candidateChunk(
                        seed,
                        (cellX + cellOffsetX) * END_CITY_GRID.spacing,
                        (cellZ + cellOffsetZ) * END_CITY_GRID.spacing,
                    )
                if (candidateX !in chunkX - STRUCTURE_REFERENCE_RANGE..chunkX + STRUCTURE_REFERENCE_RANGE) continue
                if (candidateZ !in chunkZ - STRUCTURE_REFERENCE_RANGE..chunkZ + STRUCTURE_REFERENCE_RANGE) continue
                val pieces = cityAt(seed, candidateX, candidateZ)
                if (pieces.isEmpty() || !pieces.boundingBox().intersects(clip)) continue
                references += CityReference(candidateX, candidateZ, pieces)
            }
        }
        val byChunk = references.associateBy { it.chunkX to it.chunkZ }
        val insertionOrder = references.sortedWith(compareBy(CityReference::chunkX, CityReference::chunkZ))
        return vanillaStructureReferenceChunkOrder(insertionOrder.map { it.chunkX to it.chunkZ }).map(byChunk::getValue)
    }

    private fun handleMarkers(
        chunk: Chunk,
        piece: EndCityPieces.Piece,
        clip: StructureBoundingBox,
        random: WorldgenRandom,
    ) {
        for (marker in piece.template.dataMarkers(piece.position, piece.rotation, StructureMirror.NONE, ZERO)) {
            val position = marker.pos
            when {
                marker.metadata.startsWith("Chest") -> {
                    val chest = StructurePos(position.x, position.y - 1, position.z)
                    if (!clip.isInside(chest)) continue
                    val blockEntity = chunk.blockEntityAt(chest.x and 15, chest.y, chest.z and 15) ?: continue
                    blockEntity.data.putString("LootTable", END_CITY_LOOT_TABLE)
                    blockEntity.data.putLong("LootTableSeed", random.nextLong())
                }
                marker.metadata.startsWith("Sentry") && clip.isInside(position) -> {
                    chunk.addGeneratedEntity(
                        ChunkGeneratedEntity(
                            type = Identifier.vanilla("shulker"),
                            x = position.x + 0.5,
                            y = position.y.toDouble(),
                            z = position.z + 0.5,
                            yaw = 0.0f,
                            pitch = 0.0f,
                        ),
                    )
                }
                marker.metadata.startsWith("Elytra") && clip.isInside(position) -> {
                    placeElytraFrame(chunk, position, piece.rotation)
                }
            }
        }
    }

    private fun placeElytraFrame(
        chunk: Chunk,
        position: StructurePos,
        rotation: StructureRotation,
    ) {
        val facing =
            when (rotation) {
                StructureRotation.NONE -> "south"
                StructureRotation.CLOCKWISE_90 -> "west"
                StructureRotation.CLOCKWISE_180 -> "north"
                StructureRotation.COUNTERCLOCKWISE_90 -> "east"
            }
        val x =
            position.x +
                if (facing == "west") {
                    0.96875
                } else if (facing == "east") {
                    0.03125
                } else {
                    0.5
                }
        val z =
            position.z +
                if (facing == "north") {
                    0.96875
                } else if (facing == "south") {
                    0.03125
                } else {
                    0.5
                }
        val yaw =
            when (facing) {
                "south" -> 0.0f
                "west" -> 90.0f
                "north" -> 180.0f
                else -> 270.0f
            }
        chunk.addGeneratedEntity(
            ChunkGeneratedEntity(
                type = Identifier.vanilla("item_frame"),
                x = x,
                y = position.y + 0.5,
                z = z,
                yaw = yaw,
                pitch = 0.0f,
                data =
                    NbtCompound().apply {
                        putString("facing", facing)
                        putString("item", "minecraft:elytra")
                    },
            ),
        )
    }

    private fun surfaceHeightAt(
        x: Int,
        z: Int,
    ): Int {
        surfaceHeightOverride?.let { return it(x, z) }
        val sampler = samplerAt(x, z)
        for (y in END_GENERATION_HEIGHT - 1 downTo 0) {
            if (sampler.sample(x and 15, y, z and 15) > 0.0) return y
        }
        return -1
    }

    private fun samplerAt(
        x: Int,
        z: Int,
    ): DensityChunkSampler {
        val key = (x shr 4) to (z shr 4)
        samplerCache.value[key]?.let { return it }
        val created = DensityChunkSampler(router.finalDensity, key.first, key.second, 0, END_GENERATION_HEIGHT, 8, 4)
        while (true) {
            val current = samplerCache.value
            current[key]?.let { return it }
            if (samplerCache.compareAndSet(current, current + (key to created))) return created
        }
    }

    private fun List<EndCityPieces.Piece>.boundingBox(): StructureBoundingBox =
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

    private fun StructureRotation.heightOffsets(): Pair<Int, Int> =
        when (this) {
            StructureRotation.NONE -> 5 to 5
            StructureRotation.CLOCKWISE_90 -> -5 to 5
            StructureRotation.CLOCKWISE_180 -> -5 to -5
            StructureRotation.COUNTERCLOCKWISE_90 -> 5 to -5
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

    private data class CityCacheKey(
        val seed: Long,
        val chunkX: Int,
        val chunkZ: Int,
    )

    private data class CityReference(
        val chunkX: Int,
        val chunkZ: Int,
        val pieces: List<EndCityPieces.Piece>,
    )

    private companion object {
        const val END_GENERATION_HEIGHT = 128
        const val MIN_START_Y = 60
        const val STRUCTURE_REFERENCE_RANGE = 8
        const val END_CITY_STRUCTURE_INDEX = 2
        const val SURFACE_STRUCTURES_STEP = 4
        const val END_CITY_LOOT_TABLE = "minecraft:chests/end_city_treasure"
        val ZERO = StructurePos(0, 0, 0)
        val END_CITY_BIOMES = setOf("end_highlands", "end_midlands")
        val END_CITY_GRID =
            StructurePlacementGrid(
                spacing = 20,
                separation = 11,
                salt = 10387313,
                spreadType = StructurePlacementGrid.SpreadType.TRIANGULAR,
            )
    }
}
