package org.basaltmc.server.world.terrain

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.ChunkBlockEntity
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.structure.StructureBoundingBox
import org.basaltmc.server.world.structure.StructureMirror
import org.basaltmc.server.world.structure.StructurePlacementGrid
import org.basaltmc.server.world.structure.StructurePos
import org.basaltmc.server.world.structure.StructureRotation
import org.basaltmc.server.world.structure.StructureStartRecord
import org.basaltmc.server.world.structure.StructureTemplate
import org.basaltmc.server.world.structure.TemplateBlockProcessors
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import kotlin.math.abs

/**
 * The two shipwrecks, sunk and beached: the same structure with different template lists and
 * heightmaps. // parity: ShipwreckStructure. The height is the mean of the heightmap over the whole
 * footprint, not the column it started from, so a wreck across a slope half buries itself.
 */
public class ShipwreckStructureGenerator(
    private val biomeAt: (x: Int, y: Int, z: Int) -> String,
    private val oceanBiomes: Set<String>,
    private val beachBiomes: Set<String>,
    private val templateOf: (String) -> StructureTemplate?,
    private val surfaceHeightAt: (x: Int, z: Int) -> Int,
    private val oceanFloorAt: (x: Int, z: Int) -> Int,
) {
    /**
     * A hull is up to 32 blocks long, so it spills into its neighbours. Every chunk it touches
     * places its own part, which is what vanilla does by giving each chunk the same start and
     * clipping the write; here that means walking the nearby cells rather than only this chunk's.
     */
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

        // parity: a set with more than one structure *draws* which one, with its own
        // `setLargeFeatureSeed` random, and only then tests that one's biomes; a rejected option is
        // removed and the draw repeats. Deciding by biome instead looks equivalent and is not: the
        // two variants read different hull lists, so the draw that follows lands on another ship.
        val beached = chooseVariant(seed, chunkX, chunkZ) ?: return

        // parity: `context.random()`, a separate stream with the same seeding, so the rotation is
        // its first draw whichever variant won.
        val pick = WorldgenRandom(LegacyRandom(0L))
        pick.setLargeFeatureSeed(seed, chunkX, chunkZ)
        val rotation = StructureRotation.entries[pick.nextInt(StructureRotation.entries.size)]
        val names = if (beached) BEACHED_HULLS else OCEAN_HULLS
        val template = templateOf(names[pick.nextInt(names.size)]) ?: return

        val origin = StructurePos(chunkX * 16, 0, chunkZ * 16)
        val settled = settle(template, origin, rotation, beached, pick) ?: return
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
            if (beached) BEACHED_STRUCTURE_INDEX else SUNK_STRUCTURE_INDEX,
            SURFACE_STRUCTURES_STEP,
        )
        template.placeInto(
            chunk = chunk,
            position = settled,
            rotation = rotation,
            mirror = StructureMirror.NONE,
            pivot = PIVOT,
            clipTo = clip,
            blockProcessor = TemplateBlockProcessors.IGNORE_STRUCTURE_AND_AIR,
            random = random,
        )
        placeMarkerChests(chunk, template, settled, rotation, clip, random)
        val box = template.getBoundingBox(settled, rotation, PIVOT, StructureMirror.NONE)
        if (box.intersects(clip)) chunk.structures.add(StructureStartRecord(NAME, chunkX, chunkZ, box, listOf(box)))
    }

    /**
     * Which of the two the chunk gets, or null when neither fits its biome.
     * // parity: the weighted draw in `ChunkGenerator`, both weights one, `shipwreck` first.
     */
    private fun chooseVariant(
        seed: Long,
        chunkX: Int,
        chunkZ: Int,
    ): Boolean? {
        val random = WorldgenRandom(LegacyRandom(0L))
        random.setLargeFeatureSeed(seed, chunkX, chunkZ)
        val centreX = chunkX * 16 + 8
        val centreZ = chunkZ * 16 + 8
        // false is `shipwreck`, true is `shipwreck_beached`, in the set's own order.
        val options = mutableListOf(false, true)
        while (options.isNotEmpty()) {
            val chosen = options[random.nextInt(options.size)]
            val fits =
                if (chosen) {
                    biomeAt(centreX, surfaceHeightAt(centreX, centreZ) - 1, centreZ) in beachBiomes
                } else {
                    biomeAt(centreX, oceanFloorAt(centreX, centreZ) - 1, centreZ) in oceanBiomes
                }
            if (fits) return chosen
            options.remove(chosen)
        }
        return null
    }

    /**
     * // parity: ShipwreckPiece.postProcess. The sunk hull takes the mean ocean floor of its
     * footprint; the beached one takes the lowest surface, then sinks half its own height and one
     * to three blocks more.
     */
    private fun settle(
        template: StructureTemplate,
        origin: StructurePos,
        rotation: StructureRotation,
        beached: Boolean,
        random: WorldgenRandom,
    ): StructurePos? {
        val size = template.size
        val footprint = size.x * size.z
        if (footprint == 0) return null
        var total = 0
        var lowest = WorldHeight.MAX_Y + 1
        for (dx in 0 until size.x) {
            for (dz in 0 until size.z) {
                val height =
                    if (beached) {
                        surfaceHeightAt(origin.x + dx, origin.z + dz)
                    } else {
                        oceanFloorAt(origin.x + dx, origin.z + dz)
                    }
                total += height
                if (height < lowest) lowest = height
            }
        }
        val y =
            if (beached) {
                lowest - size.y / 2 - random.nextInt(3)
            } else {
                total / footprint
            }
        return origin.copy(y = y)
    }

    /**
     * // parity: ShipwreckPiece.handleDataMarker, which turns the three chest markers into a loot
     * table on the container **below** the marker.
     */
    private fun placeMarkerChests(
        chunk: Chunk,
        template: StructureTemplate,
        position: StructurePos,
        rotation: StructureRotation,
        clip: StructureBoundingBox,
        random: WorldgenRandom,
    ) {
        for (marker in template.dataMarkers(position, rotation, StructureMirror.NONE, PIVOT)) {
            val lootTable = MARKER_LOOT[marker.metadata] ?: continue
            val below = StructurePos(marker.pos.x, marker.pos.y - 1, marker.pos.z)
            if (!clip.isInside(below)) continue
            val stateId = chunk.getBlockStateId(below.x and 15, below.y, below.z and 15)
            val path = Blocks.byStateId(stateId)?.block?.path ?: continue
            if (path != "chest" && path != "barrel") continue
            chunk.setBlockEntity(
                ChunkBlockEntity(
                    below.x and 15,
                    below.y,
                    below.z and 15,
                    Identifier.vanilla(path),
                    NbtCompound().apply {
                        putString("LootTable", lootTable)
                        putLong("LootTableSeed", random.nextLong())
                        putCompound("components", NbtCompound())
                    },
                ),
            )
        }
    }

    public companion object {
        public const val NAME: String = "shipwreck"

        /** Eighteenth and nineteenth of the 26 of `surface_structures`, in registry order. */
        public const val SUNK_STRUCTURE_INDEX: Int = 17
        public const val BEACHED_STRUCTURE_INDEX: Int = 18
        public const val SURFACE_STRUCTURES_STEP: Int = 4

        public val GRID: StructurePlacementGrid =
            StructurePlacementGrid(spacing = 24, separation = 4, salt = 165745295)

        /** // parity: ShipwreckPieces.PIVOT. */
        private val PIVOT = StructurePos(4, 0, 15)

        /** A hull is at most 32 blocks in x or z, so it never reaches further than two chunks. */
        private const val REACH_CHUNKS = 3

        private val MARKER_LOOT =
            mapOf(
                "map_chest" to "minecraft:chests/shipwreck_map",
                "treasure_chest" to "minecraft:chests/shipwreck_treasure",
                "supply_chest" to "minecraft:chests/shipwreck_supply",
            )

        private val BEACHED_HULLS =
            listOf(
                "shipwreck/with_mast",
                "shipwreck/sideways_full",
                "shipwreck/sideways_fronthalf",
                "shipwreck/sideways_backhalf",
                "shipwreck/rightsideup_full",
                "shipwreck/rightsideup_fronthalf",
                "shipwreck/rightsideup_backhalf",
                "shipwreck/with_mast_degraded",
                "shipwreck/rightsideup_full_degraded",
                "shipwreck/rightsideup_fronthalf_degraded",
                "shipwreck/rightsideup_backhalf_degraded",
            )

        private val OCEAN_HULLS =
            listOf(
                "shipwreck/with_mast",
                "shipwreck/upsidedown_full",
                "shipwreck/upsidedown_fronthalf",
                "shipwreck/upsidedown_backhalf",
                "shipwreck/sideways_full",
                "shipwreck/sideways_fronthalf",
                "shipwreck/sideways_backhalf",
                "shipwreck/rightsideup_full",
                "shipwreck/rightsideup_fronthalf",
                "shipwreck/rightsideup_backhalf",
                "shipwreck/with_mast_degraded",
                "shipwreck/upsidedown_full_degraded",
                "shipwreck/upsidedown_fronthalf_degraded",
                "shipwreck/upsidedown_backhalf_degraded",
                "shipwreck/sideways_full_degraded",
                "shipwreck/sideways_fronthalf_degraded",
                "shipwreck/sideways_backhalf_degraded",
                "shipwreck/rightsideup_full_degraded",
                "shipwreck/rightsideup_fronthalf_degraded",
                "shipwreck/rightsideup_backhalf_degraded",
            )
    }
}
