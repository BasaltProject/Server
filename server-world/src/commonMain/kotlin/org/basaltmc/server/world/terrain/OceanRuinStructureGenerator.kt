package org.basaltmc.server.world.terrain

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.structure.BlockRot
import org.basaltmc.server.world.structure.StructureBoundingBox
import org.basaltmc.server.world.structure.StructureMirror
import org.basaltmc.server.world.structure.StructurePlacementGrid
import org.basaltmc.server.world.structure.StructurePos
import org.basaltmc.server.world.structure.StructureProcessorList
import org.basaltmc.server.world.structure.StructureRotation
import org.basaltmc.server.world.structure.StructureStartRecord
import org.basaltmc.server.world.structure.StructureTemplate
import org.basaltmc.server.world.structure.StructureTransform
import org.basaltmc.server.world.structure.TemplateBlockProcessors
import kotlin.math.abs

/**
 * The warm and cold ocean ruins, which share a structure set and differ in their templates.
 * // parity: OceanRuinStructure plus OceanRuinPieces. A cold ruin is three templates stacked at one
 * place, each rotted harder than the last, and a large ruin drags a cluster of small ones around it.
 */
public class OceanRuinStructureGenerator(
    private val biomeAt: (x: Int, y: Int, z: Int) -> String,
    private val coldBiomes: Set<String>,
    private val warmBiomes: Set<String>,
    private val templateOf: (String) -> StructureTemplate?,
    private val oceanFloorAt: (x: Int, z: Int) -> Int,
) {
    public fun generateInto(
        chunk: Chunk,
        seed: Long,
    ) {
        val cellX = chunk.chunkX.floorDiv(GRID.spacing)
        val cellZ = chunk.chunkZ.floorDiv(GRID.spacing)
        for (offsetX in -1..1) {
            for (offsetZ in -1..1) {
                val (candidateX, candidateZ) =
                    GRID.candidateChunk(seed, (cellX + offsetX) * GRID.spacing, (cellZ + offsetZ) * GRID.spacing)
                if (abs(candidateX - chunk.chunkX) > REACH_CHUNKS || abs(candidateZ - chunk.chunkZ) > REACH_CHUNKS) {
                    continue
                }
                placeFrom(chunk, seed, candidateX, candidateZ)
            }
        }
    }

    private fun placeFrom(
        chunk: Chunk,
        seed: Long,
        chunkX: Int,
        chunkZ: Int,
    ) {
        if (!GRID.isStructureChunk(seed, chunkX, chunkZ)) return
        val cold = chooseVariant(seed, chunkX, chunkZ) ?: return

        val pick = WorldgenRandom(LegacyRandom(0L))
        pick.setLargeFeatureSeed(seed, chunkX, chunkZ)
        val rotation = StructureRotation.entries[pick.nextInt(StructureRotation.entries.size)]
        val large = pick.nextFloat() <= LARGE_PROBABILITY
        val baseIntegrity = if (large) 0.9f else 0.8f
        val origin = StructurePos(chunkX * 16, PLACEHOLDER_Y, chunkZ * 16)

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
        val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        random.setFeatureSeed(
            decorationSeed,
            if (cold) COLD_STRUCTURE_INDEX else WARM_STRUCTURE_INDEX,
            SURFACE_STRUCTURES_STEP,
        )

        val boxes = mutableListOf<StructureBoundingBox>()
        boxes += addPiece(chunk, clip, random, pick, origin, rotation, cold, large, baseIntegrity)
        if (large && pick.nextFloat() <= CLUSTER_PROBABILITY) {
            boxes += addCluster(chunk, clip, random, pick, origin, rotation, cold)
        }
        val touching = boxes.filter { it.intersects(clip) }
        if (touching.isEmpty()) return
        val whole = StructureStartRecord.of(NAME, chunkX, chunkZ, boxes) ?: return
        chunk.structures.add(StructureStartRecord(NAME, chunkX, chunkZ, whole.fullBounds, touching))
    }

    /** // parity: addPiece. A warm ruin is one template; a cold one is three, rotted 0.9, 0.7, 0.5. */
    private fun addPiece(
        chunk: Chunk,
        clip: StructureBoundingBox,
        random: WorldgenRandom,
        pick: WorldgenRandom,
        position: StructurePos,
        rotation: StructureRotation,
        cold: Boolean,
        large: Boolean,
        baseIntegrity: Float,
    ): List<StructureBoundingBox> {
        val names: List<Pair<String, Float>> =
            if (cold) {
                val bricks = if (large) BIG_BRICK else BRICK
                val cracked = if (large) BIG_CRACKED else CRACKED
                val mossy = if (large) BIG_MOSSY else MOSSY
                val index = pick.nextInt(bricks.size)
                listOf(bricks[index] to baseIntegrity, cracked[index] to 0.7f, mossy[index] to 0.5f)
            } else {
                val warm = if (large) BIG_WARM else WARM
                listOf(warm[pick.nextInt(warm.size)] to baseIntegrity)
            }
        val boxes = mutableListOf<StructureBoundingBox>()
        for ((name, integrity) in names) {
            val template = templateOf(name) ?: continue
            val settled = settle(chunk, template, position, rotation)
            template.placeInto(
                chunk = chunk,
                position = settled,
                rotation = rotation,
                mirror = StructureMirror.NONE,
                pivot = ZERO,
                clipTo = clip,
                processors = StructureProcessorList(emptyList(), BlockRot(integrity, null)),
                blockProcessor = TemplateBlockProcessors.IGNORE_STRUCTURE_AND_AIR,
                random = random,
                settingsRandom = random,
            )
            boxes += template.getBoundingBox(settled, rotation, ZERO, StructureMirror.NONE)
        }
        return boxes
    }

    /**
     * // parity: addClusterRuins. Eight candidate spots are drawn around the parent, four to eight
     * of them are taken at random, and one is dropped if its box would touch the parent's.
     */
    private fun addCluster(
        chunk: Chunk,
        clip: StructureBoundingBox,
        random: WorldgenRandom,
        pick: WorldgenRandom,
        parent: StructurePos,
        parentRotation: StructureRotation,
        cold: Boolean,
    ): List<StructureBoundingBox> {
        val parentOrigin = StructurePos(parent.x, PLACEHOLDER_Y, parent.z)
        val parentCorner =
            StructureTransform.transform(StructurePos(15, 0, 15), StructureMirror.NONE, parentRotation, ZERO) +
                parentOrigin
        val parentBox = StructureBoundingBox.fromCorners(parentOrigin, parentCorner)
        val bottomLeft =
            StructurePos(
                minOf(parentOrigin.x, parentCorner.x),
                parentOrigin.y,
                minOf(parentOrigin.z, parentCorner.z),
            )
        val spots = allPositions(pick, bottomLeft).toMutableList()
        val count = between(pick, 4, 8)
        val boxes = mutableListOf<StructureBoundingBox>()
        repeat(count) {
            if (spots.isEmpty()) return@repeat
            val spot = spots.removeAt(pick.nextInt(spots.size))
            val rotation = StructureRotation.entries[pick.nextInt(StructureRotation.entries.size)]
            val corner =
                StructureTransform.transform(StructurePos(5, 0, 6), StructureMirror.NONE, rotation, ZERO) + spot
            if (StructureBoundingBox.fromCorners(spot, corner).intersects(parentBox)) return@repeat
            boxes += addPiece(chunk, clip, random, pick, spot, rotation, cold, large = false, baseIntegrity = 0.8f)
        }
        return boxes
    }

    /** // parity: allPositions, eight spots each drawn from its own pair of ranges. */
    private fun allPositions(
        random: WorldgenRandom,
        origin: StructurePos,
    ): List<StructurePos> =
        listOf(
            origin.offset(-16 + between(random, 1, 8), 16 + between(random, 1, 7)),
            origin.offset(-16 + between(random, 1, 8), between(random, 1, 7)),
            origin.offset(-16 + between(random, 1, 8), -16 + between(random, 4, 8)),
            origin.offset(between(random, 1, 7), 16 + between(random, 1, 7)),
            origin.offset(between(random, 1, 7), -16 + between(random, 4, 6)),
            origin.offset(16 + between(random, 1, 7), 16 + between(random, 3, 8)),
            origin.offset(16 + between(random, 1, 7), between(random, 1, 7)),
            origin.offset(16 + between(random, 1, 7), -16 + between(random, 4, 8)),
        )

    private fun StructurePos.offset(
        dx: Int,
        dz: Int,
    ) = StructurePos(x + dx, y, z + dz)

    /**
     * // parity: OceanRuinPiece.postProcess plus its `getHeight`. The piece drops to the ocean
     * floor and then, if its footprint is mostly a hole, sinks to the bottom of that hole instead
     * of bridging it.
     */
    private fun settle(
        chunk: Chunk,
        template: StructureTemplate,
        position: StructurePos,
        rotation: StructureRotation,
    ): StructurePos {
        val floor = oceanFloorAt(position.x, position.z)
        val start = StructurePos(position.x, floor, position.z)
        val corner =
            StructureTransform.transform(
                StructurePos(template.size.x - 1, 0, template.size.z - 1),
                StructureMirror.NONE,
                rotation,
                ZERO,
            ) + start
        var lowest = 512
        var hollow = 0
        val topY = start.y - 1
        for (x in minOf(start.x, corner.x)..maxOf(start.x, corner.x)) {
            for (z in minOf(start.z, corner.z)..maxOf(start.z, corner.z)) {
                var y = start.y - 1
                while (y > WorldHeight.MIN_Y + 1 && isOpen(chunk, x, y, z)) y--
                if (y < lowest) lowest = y
                if (y < topY - 2) hollow++
            }
        }
        val width = abs(start.x - corner.x)
        val settledY = if (topY - lowest > 2 && hollow > width - 2) lowest + 1 else start.y
        return StructurePos(position.x, settledY, position.z)
    }

    private fun isOpen(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean {
        val stateId = chunk.getDecorationBlockStateId(x, y, z)
        if (Blocks.isAir(stateId)) return true
        val path = Blocks.byStateId(stateId)?.block?.path ?: return false
        return path == "water" || path in ICE
    }

    private fun chooseVariant(
        seed: Long,
        chunkX: Int,
        chunkZ: Int,
    ): Boolean? {
        val random = WorldgenRandom(LegacyRandom(0L))
        random.setLargeFeatureSeed(seed, chunkX, chunkZ)
        val centreX = chunkX * 16 + 8
        val centreZ = chunkZ * 16 + 8
        val floor = oceanFloorAt(centreX, centreZ) - 1
        // false is `ocean_ruin_cold`, true is `ocean_ruin_warm`, in the set's own order.
        val options = mutableListOf(false, true)
        while (options.isNotEmpty()) {
            val warm = options[random.nextInt(options.size)]
            val biome = biomeAt(centreX, floor, centreZ)
            if (biome in (if (warm) warmBiomes else coldBiomes)) return !warm
            options.remove(warm)
        }
        return null
    }

    /** // parity: `Mth.nextInt(random, min, max)`, inclusive at both ends. */
    private fun between(
        random: WorldgenRandom,
        min: Int,
        max: Int,
    ): Int = random.nextInt(max - min + 1) + min

    public companion object {
        public const val NAME: String = "ocean_ruin"

        /** Eighth and ninth of the 26 of `surface_structures`: cold first, then warm. */
        public const val COLD_STRUCTURE_INDEX: Int = 7
        public const val WARM_STRUCTURE_INDEX: Int = 8
        public const val SURFACE_STRUCTURES_STEP: Int = 4

        public val GRID: StructurePlacementGrid =
            StructurePlacementGrid(spacing = 20, separation = 8, salt = 14357621)

        private const val LARGE_PROBABILITY = 0.3f
        private const val CLUSTER_PROBABILITY = 0.9f

        /** // parity: the piece is built at y 90 and settled afterwards. */
        private const val PLACEHOLDER_Y = 90
        private const val REACH_CHUNKS = 3

        private val ZERO = StructurePos(0, 0, 0)
        private val ICE = setOf("ice", "packed_ice", "blue_ice", "frosted_ice")

        private fun ruins(
            prefix: String,
            count: Int,
        ) = (1..count).map { "underwater_ruin/${prefix}_$it" }

        private val WARM = ruins("warm", 8)
        private val BRICK = ruins("brick", 8)
        private val CRACKED = ruins("cracked", 8)
        private val MOSSY = ruins("mossy", 8)

        /** The large lists are 1, 2, 3 and 8: the jar skips 4 to 7 for the cold ones. */
        private val BIG_BRICK = listOf(1, 2, 3, 8).map { "underwater_ruin/big_brick_$it" }
        private val BIG_CRACKED = listOf(1, 2, 3, 8).map { "underwater_ruin/big_cracked_$it" }
        private val BIG_MOSSY = listOf(1, 2, 3, 8).map { "underwater_ruin/big_mossy_$it" }
        private val BIG_WARM = (4..7).map { "underwater_ruin/big_warm_$it" }
    }
}
