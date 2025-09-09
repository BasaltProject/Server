package org.basaltmc.server.world.terrain

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.ChunkBlockEntity
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.structure.StructureBoundingBox
import org.basaltmc.server.world.structure.StructurePlacementGrid
import org.basaltmc.server.world.structure.StructureStartRecord
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound

/**
 * The buried treasure: one chest, sunk until it finds stone, with a shell of soft blocks around it.
 * // parity: BuriedTreasureStructure. The piece is one block at offset 9 in both axes, so it never
 * leaves its chunk; the walk down is what buries a chest that lands over a sea cave.
 */
public class BuriedTreasureStructureGenerator(
    private val biomeAt: (x: Int, y: Int, z: Int) -> String,
    private val beachBiomes: Set<String>,
    private val surfaceHeightAt: (x: Int, z: Int) -> Int,
) {
    public fun generateInto(
        chunk: Chunk,
        seed: Long,
    ) {
        val chunkX = chunk.chunkX
        val chunkZ = chunk.chunkZ
        if (!GRID.isStructureChunk(seed, chunkX, chunkZ)) return
        // parity: onTopOfChunkCenter over OCEAN_FLOOR_WG, which is where the biome is tested.
        val centreX = chunkX * 16 + 8
        val centreZ = chunkZ * 16 + 8
        if (biomeAt(centreX, surfaceHeightAt(centreX, centreZ) - 1, centreZ) !in beachBiomes) return

        val x = chunkX * 16 + PIECE_OFFSET
        val z = chunkZ * 16 + PIECE_OFFSET
        var y = chunk.heightAt(x and 15, z and 15, Chunk.HeightmapType.OCEAN_FLOOR)
        while (y > WorldHeight.MIN_Y) {
            val below = chunk.getBlockStateId(x and 15, y - 1, z and 15)
            if (Blocks.byStateId(below)?.block?.path in FLOORS) {
                val current = chunk.getBlockStateId(x and 15, y, z and 15)
                val soft = if (!Blocks.isAir(current) && !isLiquid(current)) current else SAND
                shellAround(chunk, x, y, z, below, soft)
                placeChest(chunk, seed, x, y, z)
                chunk.structures.add(
                    StructureStartRecord(
                        NAME,
                        chunkX,
                        chunkZ,
                        StructureBoundingBox(x, y, z, x, y, z),
                        listOf(
                            StructureBoundingBox(x, y, z, x, y, z),
                        ),
                    ),
                )
                return
            }
            y--
        }
    }

    /**
     * // parity: the six-direction shell. A neighbour that is air or liquid gets the floor block
     * when what is under *it* is also open (so the chest is not left over a hole), and the soft
     * block otherwise. Up never gets the floor treatment, which is the jar's own exception.
     */
    private fun shellAround(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
        floorStateId: Int,
        softStateId: Int,
    ) {
        for ((dx, dy, dz) in NEIGHBOURS) {
            val nx = x + dx
            val ny = y + dy
            val nz = z + dz
            if (ny !in WorldHeight.MIN_Y..WorldHeight.MAX_Y) continue
            val state = chunk.getDecorationBlockStateId(nx, ny, nz)
            if (!Blocks.isAir(state) && !isLiquid(state)) continue
            val underneath = chunk.getDecorationBlockStateId(nx, ny - 1, nz)
            val open = Blocks.isAir(underneath) || isLiquid(underneath)
            val replacement = if (open && dy != 1) floorStateId else softStateId
            chunk.setDecorationBlockStateId(nx, ny, nz, replacement, replaceAnyExternalState = true)
        }
    }

    /** // parity: StructurePiece.createChest with an explicit position, plus `reorient`. */
    private fun placeChest(
        chunk: Chunk,
        seed: Long,
        x: Int,
        y: Int,
        z: Int,
    ) {
        if (Blocks.byStateId(chunk.getBlockStateId(x and 15, y, z and 15))?.block?.path == "chest") return
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        random.setFeatureSeed(decorationSeed, STRUCTURE_INDEX, UNDERGROUND_STRUCTURES_STEP)
        val facing = reorient(chunk, x, y, z)
        val stateId =
            Blocks.stateIdOf(
                Identifier.vanilla("chest"),
                mapOf("facing" to facing, "type" to "single", "waterlogged" to "false"),
            ) ?: return
        chunk.setBlockStateId(x and 15, y, z and 15, stateId)
        chunk.setBlockEntity(
            ChunkBlockEntity(
                x and 15,
                y,
                z and 15,
                Identifier.vanilla("chest"),
                NbtCompound().apply {
                    putString("LootTable", "minecraft:chests/buried_treasure")
                    putLong("LootTableSeed", random.nextLong())
                    putCompound("components", NbtCompound())
                },
            ),
        )
    }

    /**
     * // parity: StructurePiece.reorient. One solid horizontal neighbour turns the chest away from
     * it; two or more and it falls through to walking the default facing around until it finds a
     * side that is not solid.
     */
    private fun reorient(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ): String {
        var solid: String? = null
        for (direction in HORIZONTAL) {
            val (dx, dz) = OFFSETS.getValue(direction)
            val state = chunk.getDecorationBlockStateId(x + dx, y, z + dz)
            if (Blocks.byStateId(state)?.block?.path == "chest") return DEFAULT_FACING
            if (!BlockShapes.isSolidRender(state)) continue
            if (solid != null) {
                solid = null
                break
            }
            solid = direction
        }
        if (solid != null) return OPPOSITE.getValue(solid)

        var facing = DEFAULT_FACING
        repeat(3) { step ->
            val (dx, dz) = OFFSETS.getValue(facing)
            if (!BlockShapes.isSolidRender(chunk.getDecorationBlockStateId(x + dx, y, z + dz))) return facing
            facing = if (step == 1) CLOCKWISE.getValue(facing) else OPPOSITE.getValue(facing)
        }
        return facing
    }

    private fun isLiquid(stateId: Int): Boolean {
        val path = Blocks.byStateId(stateId)?.block?.path
        return path == "water" || path == "lava"
    }

    public companion object {
        public const val NAME: String = "buried_treasure"

        /** First of the five structures of `underground_structures`, in registry order. */
        public const val STRUCTURE_INDEX: Int = 0
        public const val UNDERGROUND_STRUCTURES_STEP: Int = 3

        public val GRID: StructurePlacementGrid =
            StructurePlacementGrid(
                spacing = 1,
                separation = 0,
                salt = 0,
                frequency = 0.01f,
                frequencyReduction = StructurePlacementGrid.FrequencyReduction.LEGACY_TYPE_2,
            )

        /** The piece sits at offset 9 in both axes, which is why it never crosses a chunk edge. */
        private const val PIECE_OFFSET = 9
        private const val DEFAULT_FACING = "north"

        private val SAND = Blocks.REGISTRY.byIdentifier(Identifier.vanilla("sand"))?.stateId ?: Blocks.AIR.stateId
        private val FLOORS = setOf("sandstone", "stone", "andesite", "granite", "diorite")
        private val HORIZONTAL = listOf("north", "east", "south", "west")
        private val OFFSETS =
            mapOf("north" to (0 to -1), "east" to (1 to 0), "south" to (0 to 1), "west" to (-1 to 0))
        private val OPPOSITE = mapOf("north" to "south", "south" to "north", "east" to "west", "west" to "east")
        private val CLOCKWISE = mapOf("north" to "east", "east" to "south", "south" to "west", "west" to "north")

        /** // parity: `Direction.values()`, down first and up second. */
        private val NEIGHBOURS =
            listOf(
                Triple(0, -1, 0),
                Triple(0, 1, 0),
                Triple(0, 0, -1),
                Triple(0, 0, 1),
                Triple(-1, 0, 0),
                Triple(1, 0, 0),
            )
    }
}
