package org.basaltmc.server.world.terrain

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.hasFluid
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.ChunkBlockEntity
import org.basaltmc.server.world.ChunkGeneratedEntity
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.structure.MansionGrid
import org.basaltmc.server.world.structure.MansionPiece
import org.basaltmc.server.world.structure.MansionPlacer
import org.basaltmc.server.world.structure.MansionRoomFiller
import org.basaltmc.server.world.structure.StructureBoundingBox
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
 * The woodland mansion. // parity: WoodlandMansionStructure. Ninety blocks across at its widest, so
 * it reaches six chunks out from the one it starts in and every chunk it touches lays its own part.
 */
public class WoodlandMansionStructureGenerator(
    private val biomeAt: (x: Int, y: Int, z: Int) -> String,
    private val mansionBiomes: Set<String>,
    private val templateOf: (String) -> StructureTemplate?,
    private val surfaceHeightAt: (x: Int, z: Int) -> Int,
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

        val startRandom = WorldgenRandom(LegacyRandom(0L))
        startRandom.setLargeFeatureSeed(seed, chunkX, chunkZ)

        val centreX = chunkX * 16 + 8
        val centreZ = chunkZ * 16 + 8
        if (biomeAt(centreX, surfaceHeightAt(centreX, centreZ) - 1, centreZ) !in mansionBiomes) return

        // parity: the rotation is drawn before the ground is looked at, so a mansion the ground
        // rejects still costs the roll.
        val rotation = StructureRotation.entries[startRandom.nextInt(StructureRotation.entries.size)]
        val origin = lowestCornerOf(chunkX, chunkZ, rotation) ?: return

        val pieces = mutableListOf<MansionPiece>()
        val grid = MansionGrid(startRandom)
        MansionPlacer(startRandom, MansionRoomFiller(startRandom)).createMansion(origin, rotation, pieces, grid)
        if (pieces.isEmpty()) return

        val clip = chunkClip(chunk.chunkX, chunk.chunkZ)
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        random.setFeatureSeed(decorationSeed, STRUCTURE_INDEX, SURFACE_STRUCTURES_STEP)

        var whole: StructureBoundingBox? = null
        val here = mutableListOf<StructureBoundingBox>()
        for (piece in pieces) {
            val template = templateOf(TEMPLATE_PREFIX + piece.template) ?: continue
            val box = template.getBoundingBox(piece.position, piece.rotation, PIVOT, piece.mirror)
            whole = whole?.let { union(it, box) } ?: box
            if (!box.intersects(clip)) continue
            here += box
            template.placeInto(
                chunk = chunk,
                position = piece.position,
                rotation = piece.rotation,
                mirror = piece.mirror,
                pivot = PIVOT,
                clipTo = clip,
                blockProcessor = TemplateBlockProcessors.IGNORE_STRUCTURE_BLOCK,
                random = random,
            )
            placeMarkers(chunk, template, piece, clip, random)
        }
        val bounds = whole ?: return
        foundation(chunk, clip, bounds, here)
        if (here.isNotEmpty()) {
            chunk.structures.add(StructureStartRecord(NAME, chunkX, chunkZ, bounds, here))
        }
    }

    /**
     * // parity: the `Chest*` and mob markers. The chest faces the way its marker names once the
     * house's own rotation is applied; a mob marker leaves air behind it.
     */
    private fun placeMarkers(
        chunk: Chunk,
        template: StructureTemplate,
        piece: MansionPiece,
        clip: StructureBoundingBox,
        random: WorldgenRandom,
    ) {
        for (marker in template.dataMarkers(piece.position, piece.rotation, piece.mirror, PIVOT)) {
            if (!clip.isInside(marker.pos)) continue
            val localX = marker.pos.x and 15
            val localZ = marker.pos.z and 15
            when {
                marker.metadata.startsWith("Chest") -> {
                    val facing = CHEST_FACINGS[marker.metadata] ?: "north"
                    val turned = turn(facing, piece.rotation)
                    val stateId =
                        Blocks.stateIdOf(
                            Identifier.vanilla("chest"),
                            mapOf("facing" to turned, "type" to "single", "waterlogged" to "false"),
                        ) ?: continue
                    chunk.setBlockStateId(localX, marker.pos.y, localZ, stateId)
                    chunk.setBlockEntity(
                        ChunkBlockEntity(
                            localX,
                            marker.pos.y,
                            localZ,
                            Identifier.vanilla("chest"),
                            NbtCompound().apply {
                                putString("LootTable", "minecraft:chests/woodland_mansion")
                                putLong("LootTableSeed", random.nextLong())
                                putCompound("components", NbtCompound())
                            },
                        ),
                    )
                }
                marker.metadata == "Mage" || marker.metadata == "Warrior" -> {
                    val type = if (marker.metadata == "Mage") "evoker" else "vindicator"
                    chunk.setBlockStateId(localX, marker.pos.y, localZ, Blocks.AIR.stateId)
                    addResident(chunk, marker.pos, type)
                }
                marker.metadata == "Group of Allays" -> {
                    // parity: one to three of them, and the count comes off the level's own random,
                    // which no seed reproduces; a positional draw stands in.
                    val count =
                        LegacyRandom(0L).forkPositional().at(marker.pos.x, marker.pos.y, marker.pos.z).nextInt(3) + 1
                    chunk.setBlockStateId(localX, marker.pos.y, localZ, Blocks.AIR.stateId)
                    repeat(count) { addResident(chunk, marker.pos, "allay") }
                }
                else -> chunk.setBlockStateId(localX, marker.pos.y, localZ, Blocks.AIR.stateId)
            }
        }
    }

    private fun addResident(
        chunk: Chunk,
        position: StructurePos,
        type: String,
    ) {
        chunk.addGeneratedEntity(
            ChunkGeneratedEntity(
                type = Identifier.vanilla(type),
                x = position.x + 0.5,
                y = position.y.toDouble(),
                z = position.z + 0.5,
                yaw = 0.0f,
                pitch = 0.0f,
                data = NbtCompound().apply { putBoolean("PersistenceRequired", true) },
            ),
        )
    }

    /**
     * // parity: WoodlandMansionStructure.afterPlace. Every column of this chunk that stands under
     * the house is carried down in cobblestone until it meets something solid, so a mansion on a
     * slope grows its own foundation.
     */
    private fun foundation(
        chunk: Chunk,
        clip: StructureBoundingBox,
        bounds: StructureBoundingBox,
        here: List<StructureBoundingBox>,
    ) {
        if (here.isEmpty()) return
        val yStart = bounds.minY
        for (x in clip.minX..clip.maxX) {
            for (z in clip.minZ..clip.maxZ) {
                if (yStart !in WorldHeight.MIN_Y..WorldHeight.MAX_Y) continue
                val standing = chunk.getBlockStateId(x and 15, yStart, z and 15)
                if (Blocks.isAir(standing)) continue
                val position = StructurePos(x, yStart, z)
                if (!bounds.isInside(position) || here.none { it.isInside(position) }) continue
                var y = yStart - 1
                while (y > WorldHeight.MIN_Y) {
                    val below = chunk.getBlockStateId(x and 15, y, z and 15)
                    if (!Blocks.isAir(below) && !Blocks.hasFluid(below)) break
                    chunk.setBlockStateId(x and 15, y, z and 15, COBBLESTONE)
                    y--
                }
            }
        }
    }

    /**
     * // parity: getLowestYIn5by5BoxOffset7Blocks. The corner box is measured from the seventh block
     * of the chunk and leans the way the house is turned, and a mansion below y 60 is refused.
     */
    private fun lowestCornerOf(
        chunkX: Int,
        chunkZ: Int,
        rotation: StructureRotation,
    ): StructurePos? {
        var offsetX = 5
        var offsetZ = 5
        when (rotation) {
            StructureRotation.CLOCKWISE_90 -> offsetX = -5
            StructureRotation.CLOCKWISE_180 -> {
                offsetX = -5
                offsetZ = -5
            }
            StructureRotation.COUNTERCLOCKWISE_90 -> offsetZ = -5
            else -> Unit
        }
        val blockX = chunkX * 16 + 7
        val blockZ = chunkZ * 16 + 7

        // parity: getFirstOccupiedHeight is the base height *minus one*, the last solid block rather
        // than the first free one above it, which is what puts the mansion's floor on the ground.
        fun corner(
            x: Int,
            z: Int,
        ) = surfaceHeightAt(x, z) - 1
        val lowest =
            minOf(
                minOf(corner(blockX, blockZ), corner(blockX, blockZ + offsetZ)),
                minOf(corner(blockX + offsetX, blockZ), corner(blockX + offsetX, blockZ + offsetZ)),
            )
        return if (lowest < MIN_GROUND) null else StructurePos(blockX, lowest, blockZ)
    }

    private fun union(
        a: StructureBoundingBox,
        b: StructureBoundingBox,
    ): StructureBoundingBox =
        StructureBoundingBox(
            minOf(a.minX, b.minX),
            minOf(a.minY, b.minY),
            minOf(a.minZ, b.minZ),
            maxOf(a.maxX, b.maxX),
            maxOf(a.maxY, b.maxY),
            maxOf(a.maxZ, b.maxZ),
        )

    private fun chunkClip(
        chunkX: Int,
        chunkZ: Int,
    ): StructureBoundingBox =
        StructureBoundingBox(
            chunkX * 16,
            WorldHeight.MIN_Y,
            chunkZ * 16,
            chunkX * 16 + 15,
            WorldHeight.MAX_Y,
            chunkZ * 16 + 15,
        )

    private fun turn(
        facing: String,
        rotation: StructureRotation,
    ): String {
        var result = facing
        repeat(QUARTER_TURNS.getValue(rotation)) { result = CLOCKWISE.getValue(result) }
        return result
    }

    public companion object {
        public const val NAME: String = "mansion"

        /** Sixth of the 26 structures of `surface_structures`, in registry order. */
        public const val STRUCTURE_INDEX: Int = 5

        private const val SURFACE_STRUCTURES_STEP = 4
        private const val REACH_CHUNKS = 7
        private const val MIN_GROUND = 60
        private const val TEMPLATE_PREFIX = "woodland_mansion/"

        public val GRID: StructurePlacementGrid =
            StructurePlacementGrid(
                spacing = 80,
                separation = 20,
                salt = 10387319,
                spreadType = StructurePlacementGrid.SpreadType.TRIANGULAR,
            )

        private val PIVOT = StructurePos(0, 0, 0)
        private val COBBLESTONE =
            Blocks.REGISTRY.byIdentifier(Identifier.vanilla("cobblestone"))?.stateId ?: Blocks.AIR.stateId

        private val CHEST_FACINGS =
            mapOf(
                "ChestNorth" to "north",
                "ChestSouth" to "south",
                "ChestEast" to "east",
                "ChestWest" to "west",
            )

        private val CLOCKWISE = mapOf("north" to "east", "east" to "south", "south" to "west", "west" to "north")

        private val QUARTER_TURNS =
            mapOf(
                StructureRotation.NONE to 0,
                StructureRotation.CLOCKWISE_90 to 1,
                StructureRotation.CLOCKWISE_180 to 2,
                StructureRotation.COUNTERCLOCKWISE_90 to 3,
            )
    }
}
