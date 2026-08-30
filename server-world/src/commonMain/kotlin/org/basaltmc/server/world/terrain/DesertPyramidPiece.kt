package org.basaltmc.server.world.terrain

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.ChunkBlockEntity
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.structure.StructureBoundingBox
import org.basaltmc.server.world.structure.StructureDirection
import org.basaltmc.server.world.structure.StructureMirror
import org.basaltmc.server.world.structure.StructurePos
import org.basaltmc.server.world.structure.StructureRotation
import org.basaltmc.server.world.structure.StructureTemplate
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound

/**
 * // parity: DesertPyramidPiece. Twenty-one blocks square, so unlike the swamp hut it spills into its
 * neighbours and the clip decides what lands, as vanilla's `chunkBB` does. The suspicious sand is not
 * written here: [suspiciousSandPositions] only collects candidates.
 */
internal class DesertPyramidPiece(
    west: Int,
    north: Int,
    private val orientation: StructureDirection,
    private val groundOffset: Int,
) {
    var bounds: StructureBoundingBox =
        StructureBoundingBox(
            west,
            FLOOR_Y,
            north,
            west + WIDTH - 1,
            FLOOR_Y + HEIGHT - 1,
            north + DEPTH - 1,
        )
        private set

    /** World positions vanilla records as candidates for suspicious sand, in its own order. */
    val suspiciousSandPositions: MutableList<StructurePos> = mutableListOf()

    /** The one collapsed-roof block that is always suspicious, whichever else is picked. */
    var collapsedRoofPos: StructurePos? = null
        private set

    /**
     * // parity: updateHeightPositionToLowestGroundHeight, the *lowest* ground under the whole
     * footprint rather than the mean the hut uses, then dropped by up to two more.
     */
    fun settleOnGround(groundAt: (x: Int, z: Int) -> Int): Boolean {
        var lowest = WorldHeight.MAX_Y + 1
        for (z in bounds.minZ..bounds.maxZ) {
            for (x in bounds.minX..bounds.maxX) lowest = minOf(lowest, groundAt(x, z))
        }
        if (lowest > WorldHeight.MAX_Y) return false
        bounds = bounds.move(StructurePos(0, lowest - bounds.minY + groundOffset, 0))
        return true
    }

    fun place(
        chunk: Chunk,
        clip: StructureBoundingBox,
        worldSeed: Long,
        random: BasaltRandom,
    ) {
        this.chunk = chunk
        this.clip = clip
        this.worldSeed = worldSeed
        buildShell()
        buildTowers()
        buildEntrance()
        buildChambers()
        buildFloorMosaic()
        buildWallPattern()
        buildTrapChamber()
        placeChests(random)
        addCellar()
    }

    private lateinit var chunk: Chunk
    private lateinit var clip: StructureBoundingBox
    private var worldSeed: Long = 0L

    /** The stepped body, its sandstone base and the columns poured down to the ground. */
    private fun buildShell() {
        box(0, -4, 0, LAST_X, 0, LAST_Z, SANDSTONE, SANDSTONE)
        for (layer in 1..9) {
            box(layer, layer, layer, LAST_X - layer, layer, LAST_Z - layer, SANDSTONE, SANDSTONE)
            box(layer + 1, layer, layer + 1, WIDTH - 2 - layer, layer, DEPTH - 2 - layer, AIR, AIR)
        }
        for (x in 0 until WIDTH) {
            for (z in 0 until DEPTH) fillColumnDown(SANDSTONE, x, -5, z)
        }
    }

    /** The two hollow corner towers and their capped tops. */
    private fun buildTowers() {
        box(0, 0, 0, 4, 9, 4, SANDSTONE, AIR)
        box(1, 10, 1, 3, 10, 3, SANDSTONE, SANDSTONE)
        set(stairs("north"), 2, 10, 0)
        set(stairs("south"), 2, 10, 4)
        set(stairs("east"), 0, 10, 2)
        set(stairs("west"), 4, 10, 2)
        box(WIDTH - 5, 0, 0, LAST_X, 9, 4, SANDSTONE, AIR)
        box(WIDTH - 4, 10, 1, WIDTH - 2, 10, 3, SANDSTONE, SANDSTONE)
        set(stairs("north"), WIDTH - 3, 10, 0)
        set(stairs("south"), WIDTH - 3, 10, 4)
        set(stairs("east"), WIDTH - 5, 10, 2)
        set(stairs("west"), LAST_X, 10, 2)
    }

    /** The doorway between the towers and the two corridors that lead off it. */
    private fun buildEntrance() {
        box(8, 0, 0, 12, 4, 4, SANDSTONE, AIR)
        box(9, 1, 0, 11, 3, 4, AIR, AIR)
        for ((x, y) in listOf(9 to 1, 9 to 2, 9 to 3, 10 to 3, 11 to 3, 11 to 2, 11 to 1)) {
            set(CUT_SANDSTONE, x, y, 1)
        }
        box(4, 1, 1, 8, 3, 3, SANDSTONE, AIR)
        box(4, 1, 2, 8, 2, 2, AIR, AIR)
        box(12, 1, 1, 16, 3, 3, SANDSTONE, AIR)
        box(12, 1, 2, 16, 2, 2, AIR, AIR)
    }

    /** The inner rooms, their pillars, the two side shafts and the ribbed corridors. */
    private fun buildChambers() {
        box(5, 4, 5, WIDTH - 6, 4, DEPTH - 6, SANDSTONE, SANDSTONE)
        box(9, 4, 9, 11, 4, 11, AIR, AIR)
        for (x in listOf(8, 12)) {
            for (z in listOf(8, 12)) box(x, 1, z, x, 3, z, CUT_SANDSTONE, CUT_SANDSTONE)
        }
        box(1, 1, 5, 4, 4, 11, SANDSTONE, SANDSTONE)
        box(WIDTH - 5, 1, 5, WIDTH - 2, 4, 11, SANDSTONE, SANDSTONE)
        box(6, 7, 9, 6, 7, 11, SANDSTONE, SANDSTONE)
        box(WIDTH - 7, 7, 9, WIDTH - 7, 7, 11, SANDSTONE, SANDSTONE)
        box(5, 5, 9, 5, 7, 11, CUT_SANDSTONE, CUT_SANDSTONE)
        box(WIDTH - 6, 5, 9, WIDTH - 6, 7, 11, CUT_SANDSTONE, CUT_SANDSTONE)
        set(AIR, 5, 5, 10)
        set(AIR, 5, 6, 10)
        set(AIR, 6, 6, 10)
        set(AIR, WIDTH - 6, 5, 10)
        set(AIR, WIDTH - 6, 6, 10)
        set(AIR, WIDTH - 7, 6, 10)
        box(2, 4, 4, 2, 6, 4, AIR, AIR)
        box(WIDTH - 3, 4, 4, WIDTH - 3, 6, 4, AIR, AIR)
        set(stairs("north"), 2, 4, 5)
        set(stairs("north"), 2, 3, 4)
        set(stairs("north"), WIDTH - 3, 4, 5)
        set(stairs("north"), WIDTH - 3, 3, 4)
        box(1, 1, 3, 2, 2, 3, SANDSTONE, SANDSTONE)
        box(WIDTH - 3, 1, 3, WIDTH - 2, 2, 3, SANDSTONE, SANDSTONE)
        set(SANDSTONE, 1, 1, 2)
        set(SANDSTONE, WIDTH - 2, 1, 2)
        set(SANDSTONE_SLAB, 1, 2, 2)
        set(SANDSTONE_SLAB, WIDTH - 2, 2, 2)
        set(stairs("west"), 2, 1, 2)
        set(stairs("east"), WIDTH - 3, 1, 2)
        box(4, 3, 5, 4, 3, 17, SANDSTONE, SANDSTONE)
        box(WIDTH - 5, 3, 5, WIDTH - 5, 3, 17, SANDSTONE, SANDSTONE)
        box(3, 1, 5, 4, 2, 16, AIR, AIR)
        box(WIDTH - 6, 1, 5, WIDTH - 5, 2, 16, AIR, AIR)
        for (z in 5..17 step 2) {
            set(CUT_SANDSTONE, 4, 1, z)
            set(CHISELED_SANDSTONE, 4, 2, z)
            set(CUT_SANDSTONE, WIDTH - 5, 1, z)
            set(CHISELED_SANDSTONE, WIDTH - 5, 2, z)
        }
    }

    /** The terracotta eye on the floor of the main room. */
    private fun buildFloorMosaic() {
        val orange =
            listOf(
                10 to 7,
                10 to 8,
                9 to 9,
                11 to 9,
                8 to 10,
                12 to 10,
                7 to 10,
                13 to 10,
                9 to 11,
                11 to 11,
                10 to 12,
                10 to 13,
            )
        for ((x, z) in orange) set(ORANGE_TERRACOTTA, x, 0, z)
        set(BLUE_TERRACOTTA, 10, 0, 10)
    }

    /** The banded decoration on the two side walls and on the front, plus the doorway lintel. */
    private fun buildWallPattern() {
        for (x in 0..LAST_X step LAST_X) {
            for ((index, row) in SIDE_ROWS.withIndex()) {
                val y = index + 2
                set(row[0], x, y, 1)
                set(row[1], x, y, 2)
                set(row[2], x, y, 3)
            }
        }
        for (x in 2..WIDTH - 3 step WIDTH - 5) {
            for ((index, row) in SIDE_ROWS.withIndex()) {
                val y = index + 2
                set(row[0], x - 1, y, 0)
                set(row[1], x, y, 0)
                set(row[2], x + 1, y, 0)
            }
        }
        box(8, 4, 0, 12, 6, 0, CUT_SANDSTONE, CUT_SANDSTONE)
        set(AIR, 8, 6, 0)
        set(AIR, 12, 6, 0)
        set(ORANGE_TERRACOTTA, 9, 5, 0)
        set(CHISELED_SANDSTONE, 10, 5, 0)
        set(ORANGE_TERRACOTTA, 11, 5, 0)
    }

    /** The buried treasure room, its pressure plate and the TNT under it. */
    private fun buildTrapChamber() {
        box(8, -14, 8, 12, -11, 12, CUT_SANDSTONE, CUT_SANDSTONE)
        box(8, -10, 8, 12, -10, 12, CHISELED_SANDSTONE, CHISELED_SANDSTONE)
        box(8, -9, 8, 12, -9, 12, CUT_SANDSTONE, CUT_SANDSTONE)
        box(8, -8, 8, 12, -1, 12, SANDSTONE, SANDSTONE)
        box(9, -11, 9, 11, -1, 11, AIR, AIR)
        set(STONE_PRESSURE_PLATE, 10, -11, 10)
        box(9, -13, 9, 11, -13, 11, TNT, AIR)
        // The four alcoves, each an opening in the wall with its own two decorated blocks behind.
        for ((dx, dz) in HORIZONTAL_OFFSETS) {
            set(AIR, 10 + dx * 2, -11, 10 + dz * 2)
            set(AIR, 10 + dx * 2, -10, 10 + dz * 2)
            set(CHISELED_SANDSTONE, 10 + dx * 3, -10, 10 + dz * 3)
            set(CUT_SANDSTONE, 10 + dx * 3, -11, 10 + dz * 3)
        }
    }

    /** // parity: one chest per horizontal direction, each two blocks out from the plate. */
    private fun placeChests(random: BasaltRandom) {
        for ((dx, dz) in HORIZONTAL_OFFSETS) {
            createChest(random, 10 + dx * 2, -11, 10 + dz * 2)
        }
    }

    private fun addCellar() {
        addCellarStairs()
        addCellarRoom()
    }

    /**
     * The sand ramp down into the cellar. // parity: `variant` comes off the level's own random,
     * which is not derived from the world seed and so gives vanilla two different ramps for the
     * same seed. A positional draw stands in: there is no vanilla answer to match.
     */
    private fun addCellarStairs() {
        val westStairs = stairs("west")
        set(westStairs, 13, -1, 17)
        set(westStairs, 14, -2, 17)
        set(westStairs, 15, -3, 17)
        val x = CELLAR_X
        val y = CELLAR_Y
        val z = CELLAR_Z
        val variant = positionalAt(x, y, z).nextBoolean()
        for (offset in -4..0) set(SAND, x + offset, y + 4, z + 4)
        set(SAND, x - 2, y + 3, z + 4)
        set(if (variant) SAND else SANDSTONE, x - 1, y + 3, z + 4)
        set(if (!variant) SAND else SANDSTONE, x, y + 3, z + 4)
        set(SAND, x - 1, y + 2, z + 4)
        set(SANDSTONE, x, y + 2, z + 4)
        set(SAND, x, y + 1, z + 4)
    }

    private fun addCellarRoom() {
        val x = CELLAR_X
        val y = CELLAR_Y
        val z = CELLAR_Z
        for ((level, state) in listOf(y + 1 to CUT_SANDSTONE, y + 2 to CHISELED_SANDSTONE, -1 to CUT_SANDSTONE)) {
            box(x - 3, level, z - 3, x - 3, level, z + 2, state, state)
            box(x + 3, level, z - 3, x + 3, level, z + 2, state, state)
            box(x - 3, level, z - 3, x + 3, level, z - 2, state, state)
            box(x - 3, level, z + 3, x + 3, level, z + 3, state, state)
        }
        recordSandBox(x - 2, y + 1, z - 2, x + 2, y + 3, z + 2)
        placeCollapsedRoof(x - 2, y + 4, z - 2, x + 2, z + 2)
        set(BLUE_TERRACOTTA, x, y, z)
        for (dx in listOf(-1, 1)) {
            for (dz in listOf(-1, 1)) set(ORANGE_TERRACOTTA, x + dx, y, z + dz)
        }
        set(ORANGE_TERRACOTTA, x + 2, y, z)
        set(ORANGE_TERRACOTTA, x - 2, y, z)
        set(ORANGE_TERRACOTTA, x, y, z + 2)
        set(ORANGE_TERRACOTTA, x, y, z - 2)
        // The two side niches, each an orange arm with a sand plug and a decorated back.
        for (dx in listOf(1, -1)) {
            set(ORANGE_TERRACOTTA, x + dx * 3, y, z)
            recordSand(x + dx * 3, y + 1, z)
            recordSand(x + dx * 3, y + 2, z)
            set(CUT_SANDSTONE, x + dx * 4, y + 1, z)
            set(CHISELED_SANDSTONE, x + dx * 4, y + 2, z)
        }
        // The two on the Z axis are not symmetric: only the north one gets a back wall.
        for (dz in listOf(1, -1)) {
            set(ORANGE_TERRACOTTA, x, y, z + dz * 3)
            recordSand(x, y + 1, z + dz * 3)
            recordSand(x, y + 2, z + dz * 3)
        }
        set(CUT_SANDSTONE, x, y + 1, z - 4)
        set(CHISELED_SANDSTONE, x, -2, z - 4)
    }

    /**
     * // parity: placeCollapsedRoof. The material is another draw off the level's own random, so it
     * gets the same positional stand-in as the ramp; the block that is always suspicious is not,
     * and comes off the world seed exactly as vanilla forks it.
     */
    private fun placeCollapsedRoof(
        x0: Int,
        y0: Int,
        z0: Int,
        x1: Int,
        z1: Int,
    ) {
        for (x in x0..x1) {
            for (z in z0..z1) {
                set(if (positionalAt(x, y0, z).nextFloat() < COLLAPSED_SANDSTONE_CHANCE) SANDSTONE else SAND, x, y0, z)
            }
        }
        val random = LegacyRandom(worldSeed).forkPositional().at(worldX(x0, z0), worldY(y0), worldZ(x0, z0))
        val roofX = random.nextIntBetweenInclusive(x0, x1)
        val roofZ = random.nextIntBetweenInclusive(z0, z1)
        collapsedRoofPos = StructurePos(worldX(roofX, roofZ), worldY(y0), worldZ(roofX, roofZ))
    }

    private fun recordSandBox(
        x0: Int,
        y0: Int,
        z0: Int,
        x1: Int,
        y1: Int,
        z1: Int,
    ) {
        for (y in y0..y1) {
            for (x in x0..x1) {
                for (z in z0..z1) recordSand(x, y, z)
            }
        }
    }

    /** // parity: placeSand, which writes nothing: it only offers the position to the sand pass. */
    private fun recordSand(
        x: Int,
        y: Int,
        z: Int,
    ) {
        suspiciousSandPositions += StructurePos(worldX(x, z), worldY(y), worldZ(x, z))
    }

    private fun createChest(
        random: BasaltRandom,
        x: Int,
        y: Int,
        z: Int,
    ) {
        val worldX = worldX(x, z)
        val worldY = worldY(y)
        val worldZ = worldZ(x, z)
        if (!clip.isInside(StructurePos(worldX, worldY, worldZ))) return
        if (Blocks.byStateId(chunk.getDecorationBlockStateId(worldX, worldY, worldZ))?.block?.path == "chest") return
        // parity: the loot seed is drawn only when the chest is really placed, not before the clip.
        val seed = random.nextLong()
        val facing = reorient(worldX, worldY, worldZ)
        val stateId =
            Blocks.stateIdOf(
                Identifier.vanilla("chest"),
                mapOf(
                    "facing" to facing,
                    "type" to "single",
                    "waterlogged" to "false",
                ),
            )
                ?: return
        chunk.setBlockStateId(worldX and 15, worldY, worldZ and 15, stateId)
        chunk.setBlockEntity(
            ChunkBlockEntity(
                worldX and 15,
                worldY,
                worldZ and 15,
                Identifier.vanilla("chest"),
                NbtCompound().apply {
                    putString("LootTable", "minecraft:chests/desert_pyramid")
                    putLong("LootTableSeed", seed)
                    putCompound("components", NbtCompound())
                },
            ),
        )
    }

    /** // parity: StructurePiece.reorient, the chest turning its back on its one solid neighbour. */
    private fun reorient(
        x: Int,
        y: Int,
        z: Int,
    ): String {
        var solid: String? = null
        for ((direction, offset) in FACING_OFFSETS) {
            val state = chunk.getDecorationBlockStateId(x + offset.first, y, z + offset.second)
            if (Blocks.byStateId(state)?.block?.path == "chest") return "north"
            if (!BlockShapes.isSolidRender(state)) continue
            if (solid != null) return "north"
            solid = direction
        }
        return solid?.let { OPPOSITE.getValue(it) } ?: "north"
    }

    private fun box(
        x0: Int,
        y0: Int,
        z0: Int,
        x1: Int,
        y1: Int,
        z1: Int,
        edge: Int,
        fill: Int,
    ) {
        for (y in y0..y1) {
            for (x in x0..x1) {
                for (z in z0..z1) {
                    val inside = y != y0 && y != y1 && x != x0 && x != x1 && z != z0 && z != z1
                    set(if (inside) fill else edge, x, y, z)
                }
            }
        }
    }

    private fun set(
        stateId: Int,
        x: Int,
        y: Int,
        z: Int,
    ) {
        val worldY = worldY(y)
        if (worldY !in WorldHeight.MIN_Y..WorldHeight.MAX_Y) return
        val worldX = worldX(x, z)
        val worldZ = worldZ(x, z)
        if (!clip.isInside(StructurePos(worldX, worldY, worldZ))) return
        chunk.setBlockStateId(worldX and 15, worldY, worldZ and 15, transformed(stateId))
    }

    private fun fillColumnDown(
        stateId: Int,
        x: Int,
        startY: Int,
        z: Int,
    ) {
        val worldX = worldX(x, z)
        val worldZ = worldZ(x, z)
        if (worldX shr 4 != chunk.chunkX || worldZ shr 4 != chunk.chunkZ) return
        val localX = worldX and 15
        val localZ = worldZ and 15
        var y = worldY(startY)
        while (y > WorldHeight.MIN_Y + 1 && isReplaceableByStructures(chunk.getBlockStateId(localX, y, localZ))) {
            chunk.setBlockStateId(localX, y, localZ, stateId)
            y--
        }
    }

    /** // parity: StructurePiece.isReplaceableByStructures. */
    private fun isReplaceableByStructures(stateId: Int): Boolean {
        if (Blocks.isAir(stateId)) return true
        return Blocks.byStateId(stateId)?.block?.path in REPLACEABLE
    }

    /** A per-position stand-in for vanilla's level random, which no seed can reproduce. */
    private fun positionalAt(
        x: Int,
        y: Int,
        z: Int,
    ): BasaltRandom = LegacyRandom(worldSeed).forkPositional().at(worldX(x, z), worldY(y), worldZ(x, z))

    private val mirror =
        when (orientation) {
            StructureDirection.SOUTH, StructureDirection.WEST -> StructureMirror.LEFT_RIGHT
            else -> StructureMirror.NONE
        }
    private val rotation =
        when (orientation) {
            StructureDirection.WEST, StructureDirection.EAST -> StructureRotation.CLOCKWISE_90
            else -> StructureRotation.NONE
        }

    private fun transformed(stateId: Int): Int {
        val properties = Blocks.byStateId(stateId)?.properties ?: return stateId
        if (properties.isEmpty()) return stateId
        return TEMPLATE.transformedStateId(stateId, properties, mirror, rotation)
    }

    private fun worldX(
        x: Int,
        z: Int,
    ): Int =
        when (orientation) {
            StructureDirection.NORTH, StructureDirection.SOUTH -> bounds.minX + x
            StructureDirection.WEST -> bounds.maxX - z
            StructureDirection.EAST -> bounds.minX + z
            else -> x
        }

    private fun worldY(y: Int): Int = y + bounds.minY

    private fun worldZ(
        x: Int,
        z: Int,
    ): Int =
        when (orientation) {
            StructureDirection.NORTH -> bounds.maxZ - z
            StructureDirection.SOUTH -> bounds.minZ + z
            StructureDirection.WEST, StructureDirection.EAST -> bounds.minZ + x
            else -> z
        }

    companion object {
        const val WIDTH: Int = 21
        const val DEPTH: Int = 21
        const val HEIGHT: Int = 15

        private const val LAST_X = WIDTH - 1
        private const val LAST_Z = DEPTH - 1
        private const val FLOOR_Y = 64
        private const val COLLAPSED_SANDSTONE_CHANCE = 0.33f
        private const val CELLAR_X = 16
        private const val CELLAR_Y = -4
        private const val CELLAR_Z = 13

        private val TEMPLATE = StructureTemplate()

        private fun state(path: String): Int =
            Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: Blocks.AIR.stateId

        private fun stairs(facing: String): Int =
            Blocks.stateIdOf(
                Identifier.vanilla("sandstone_stairs"),
                mapOf("facing" to facing, "half" to "bottom", "shape" to "straight", "waterlogged" to "false"),
            ) ?: state("sandstone_stairs")

        private val AIR = Blocks.AIR.stateId
        private val SANDSTONE = state("sandstone")
        private val CUT_SANDSTONE = state("cut_sandstone")
        private val CHISELED_SANDSTONE = state("chiseled_sandstone")
        private val SAND = state("sand")
        private val TNT = state("tnt")
        private val ORANGE_TERRACOTTA = state("orange_terracotta")
        private val BLUE_TERRACOTTA = state("blue_terracotta")
        private val STONE_PRESSURE_PLATE = state("stone_pressure_plate")
        private val SANDSTONE_SLAB =
            Blocks.stateIdOf(
                Identifier.vanilla("sandstone_slab"),
                mapOf("type" to "bottom", "waterlogged" to "false"),
            ) ?: state("sandstone_slab")

        /** The seven rows of the wall band, from y 2 up to y 8, each three blocks wide. */
        private val SIDE_ROWS: List<IntArray> by lazy {
            listOf(
                intArrayOf(CUT_SANDSTONE, ORANGE_TERRACOTTA, CUT_SANDSTONE),
                intArrayOf(CUT_SANDSTONE, ORANGE_TERRACOTTA, CUT_SANDSTONE),
                intArrayOf(ORANGE_TERRACOTTA, CHISELED_SANDSTONE, ORANGE_TERRACOTTA),
                intArrayOf(CUT_SANDSTONE, ORANGE_TERRACOTTA, CUT_SANDSTONE),
                intArrayOf(ORANGE_TERRACOTTA, CHISELED_SANDSTONE, ORANGE_TERRACOTTA),
                intArrayOf(ORANGE_TERRACOTTA, ORANGE_TERRACOTTA, ORANGE_TERRACOTTA),
                intArrayOf(CUT_SANDSTONE, CUT_SANDSTONE, CUT_SANDSTONE),
            )
        }

        /** // parity: Direction.Plane.HORIZONTAL, which is north, east, south, west in that order. */
        private val HORIZONTAL_OFFSETS = listOf(0 to -1, 1 to 0, 0 to 1, -1 to 0)

        private val FACING_OFFSETS =
            listOf("north" to (0 to -1), "east" to (1 to 0), "south" to (0 to 1), "west" to (-1 to 0))

        private val OPPOSITE = mapOf("north" to "south", "south" to "north", "east" to "west", "west" to "east")

        private val REPLACEABLE = setOf("water", "lava", "glow_lichen", "seagrass", "tall_seagrass")
    }
}
