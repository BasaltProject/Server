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
import org.basaltmc.server.world.structure.StructureTransform
import org.basaltmc.server.world.structure.TemplateBlockProcessors
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound

/**
 * The igloo, and half the time the ladder shaft and laboratory under it. // parity: IglooStructure
 * plus IglooPieces. The shaft is the middle template repeated per level, so the whole thing is one
 * position shifted three blocks at a time rather than three separate structures.
 */
public class IglooStructureGenerator(
    private val biomeAt: (x: Int, y: Int, z: Int) -> String,
    private val iglooBiomes: Set<String>,
    private val templateOf: (String) -> StructureTemplate?,
    private val surfaceHeightAt: (x: Int, z: Int) -> Int,
) {
    public fun generateInto(
        chunk: Chunk,
        seed: Long,
    ) {
        val chunkX = chunk.chunkX
        val chunkZ = chunk.chunkZ
        if (!GRID.isStructureChunk(seed, chunkX, chunkZ)) return
        val centreX = chunkX * 16 + 8
        val centreZ = chunkZ * 16 + 8
        if (biomeAt(centreX, surfaceHeightAt(centreX, centreZ) - 1, centreZ) !in iglooBiomes) return

        val pick = WorldgenRandom(LegacyRandom(0L))
        pick.setLargeFeatureSeed(seed, chunkX, chunkZ)
        val start = StructurePos(chunkX * 16, GENERATION_HEIGHT, chunkZ * 16)
        val rotation = StructureRotation.entries[pick.nextInt(StructureRotation.entries.size)]

        // parity: the basement is decided first and its pieces are added before the igloo itself,
        // which is the order they are then written in.
        val pieces = mutableListOf<Pair<String, Int>>()
        if (pick.nextDouble() < BASEMENT_CHANCE) {
            val depth = pick.nextInt(8) + 4
            pieces += LABORATORY to depth * 3
            for (level in 0 until depth - 1) pieces += LADDER to level * 3
        }
        pieces += TOP to 0

        val clip =
            StructureBoundingBox(
                chunkX * 16,
                WorldHeight.MIN_Y,
                chunkZ * 16,
                chunkX * 16 + 15,
                WorldHeight.MAX_Y,
                chunkZ * 16 + 15,
            )
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunkX * 16, chunkZ * 16)
        random.setFeatureSeed(decorationSeed, STRUCTURE_INDEX, SURFACE_STRUCTURES_STEP)

        val boxes = mutableListOf<StructureBoundingBox>()
        for ((name, depth) in pieces) {
            boxes += place(chunk, clip, random, name, start, rotation, depth) ?: continue
        }
        if (boxes.isEmpty()) return
        val whole = StructureStartRecord.of(NAME, chunkX, chunkZ, boxes) ?: return
        val here = boxes.filter { it.intersects(clip) }
        if (here.isNotEmpty()) {
            chunk.structures.add(StructureStartRecord(NAME, chunkX, chunkZ, whole.fullBounds, here))
        }
    }

    private fun place(
        chunk: Chunk,
        clip: StructureBoundingBox,
        random: WorldgenRandom,
        name: String,
        start: StructurePos,
        rotation: StructureRotation,
        depth: Int,
    ): StructureBoundingBox? {
        val template = templateOf(name) ?: return null
        val pivot = PIVOTS.getValue(name)
        val offset = OFFSETS.getValue(name)
        val placed = StructurePos(start.x + offset.x, start.y + offset.y - depth, start.z + offset.z)

        // parity: the whole igloo settles on the surface at its *entrance* column, not its corner.
        val entrance =
            StructureTransform.transform(
                StructurePos(3 - offset.x, 0, -offset.z),
                StructureMirror.NONE,
                rotation,
                pivot,
            ) + placed
        val surface = surfaceHeightAt(entrance.x, entrance.z)
        val settled = StructurePos(placed.x, placed.y + surface - GENERATION_HEIGHT - 1, placed.z)

        template.placeInto(
            chunk = chunk,
            position = settled,
            rotation = rotation,
            mirror = StructureMirror.NONE,
            pivot = pivot,
            clipTo = clip,
            blockProcessor = TemplateBlockProcessors.IGNORE_STRUCTURE_BLOCK,
            random = random,
        )
        placeChestMarker(chunk, template, settled, rotation, pivot, clip, random)
        if (name == TOP) closeTrapDoorWithoutShaft(chunk, settled, rotation, pivot, clip)
        return template.getBoundingBox(settled, rotation, pivot, StructureMirror.NONE)
    }

    /** // parity: handleDataMarker, which clears the marker and puts the loot in the chest below. */
    private fun placeChestMarker(
        chunk: Chunk,
        template: StructureTemplate,
        position: StructurePos,
        rotation: StructureRotation,
        pivot: StructurePos,
        clip: StructureBoundingBox,
        random: WorldgenRandom,
    ) {
        for (marker in template.dataMarkers(position, rotation, StructureMirror.NONE, pivot)) {
            if (marker.metadata != "chest") continue
            if (!clip.isInside(marker.pos)) continue
            chunk.setBlockStateId(marker.pos.x and 15, marker.pos.y, marker.pos.z and 15, Blocks.AIR.stateId)
            val below = StructurePos(marker.pos.x, marker.pos.y - 1, marker.pos.z)
            val stateId = chunk.getBlockStateId(below.x and 15, below.y, below.z and 15)
            if (Blocks.byStateId(stateId)?.block?.path != "chest") continue
            chunk.setBlockEntity(
                ChunkBlockEntity(
                    below.x and 15,
                    below.y,
                    below.z and 15,
                    Identifier.vanilla("chest"),
                    NbtCompound().apply {
                        putString("LootTable", "minecraft:chests/igloo_chest")
                        putLong("LootTableSeed", random.nextLong())
                        putCompound("components", NbtCompound())
                    },
                ),
            )
        }
    }

    /**
     * // parity: the trapdoor square is filled with snow when there is no ladder under it, which is
     * what closes the floor of an igloo that drew no basement.
     */
    private fun closeTrapDoorWithoutShaft(
        chunk: Chunk,
        position: StructurePos,
        rotation: StructureRotation,
        pivot: StructurePos,
        clip: StructureBoundingBox,
    ) {
        val trapDoor =
            StructureTransform.transform(StructurePos(3, 0, 5), StructureMirror.NONE, rotation, pivot) + position
        if (!clip.isInside(trapDoor)) return
        val below = chunk.getDecorationBlockStateId(trapDoor.x, trapDoor.y - 1, trapDoor.z)
        if (Blocks.isAir(below) || Blocks.byStateId(below)?.block?.path == "ladder") return
        chunk.setBlockStateId(trapDoor.x and 15, trapDoor.y, trapDoor.z and 15, SNOW_BLOCK)
    }

    public companion object {
        public const val NAME: String = "igloo"

        /** Fourth of the 26 of `surface_structures`, in registry order. */
        public const val STRUCTURE_INDEX: Int = 3
        public const val SURFACE_STRUCTURES_STEP: Int = 4

        public val GRID: StructurePlacementGrid =
            StructurePlacementGrid(spacing = 32, separation = 8, salt = 14357618)

        private const val GENERATION_HEIGHT = 90
        private const val BASEMENT_CHANCE = 0.5

        private const val TOP = "igloo/top"
        private const val LADDER = "igloo/middle"
        private const val LABORATORY = "igloo/bottom"

        private val PIVOTS =
            mapOf(
                TOP to StructurePos(3, 5, 5),
                LADDER to StructurePos(1, 3, 1),
                LABORATORY to StructurePos(3, 6, 7),
            )

        private val OFFSETS =
            mapOf(
                TOP to StructurePos(0, 0, 0),
                LADDER to StructurePos(2, -3, 4),
                LABORATORY to StructurePos(0, -3, -2),
            )

        private val SNOW_BLOCK =
            Blocks.REGISTRY.byIdentifier(Identifier.vanilla("snow_block"))?.stateId ?: Blocks.AIR.stateId
    }
}
