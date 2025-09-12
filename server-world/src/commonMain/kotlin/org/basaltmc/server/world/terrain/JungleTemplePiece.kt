package org.basaltmc.server.world.terrain

import org.basaltmc.server.random.BasaltRandom
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
 * // parity: JungleTemplePiece. Twelve by fifteen, so each chunk writes its own part against the
 * clip. The one thing that cannot be reordered: the moss selector draws a float for every position of
 * a box, inside the chunk or not, and only then is the write clipped.
 */
internal class JungleTemplePiece(
    west: Int,
    north: Int,
    private val orientation: StructureDirection,
) {
    var bounds: StructureBoundingBox =
        makeBoundingBox(west, FLOOR_Y, north, orientation)
        private set

    /** // parity: updateAverageGroundHeight over MOTION_BLOCKING_NO_LEAVES. */
    fun settleOnGround(groundAt: (x: Int, z: Int) -> Int): Boolean {
        var total = 0
        var count = 0
        for (z in bounds.minZ..bounds.maxZ) {
            for (x in bounds.minX..bounds.maxX) {
                total += groundAt(x, z)
                count++
            }
        }
        if (count == 0) return false
        bounds = bounds.move(StructurePos(0, total / count - bounds.minY, 0))
        return true
    }

    fun place(
        chunk: Chunk,
        clip: StructureBoundingBox,
        random: BasaltRandom,
    ) {
        this.chunk = chunk
        this.clip = clip
        this.random = random
        buildShell()
        buildRoof()
        buildStairs()
        buildCellar()
        buildTripwireTrap()
        buildArrowTrap()
        buildLeverPuzzle()
    }

    private lateinit var chunk: Chunk
    private lateinit var clip: StructureBoundingBox
    private lateinit var random: BasaltRandom

    private fun buildShell() {
        mossBox(0, -4, 0, LAST_X, 0, LAST_Z)
        mossBox(2, 1, 2, 9, 2, 2)
        mossBox(2, 1, 12, 9, 2, 12)
        mossBox(2, 1, 3, 2, 2, 11)
        mossBox(9, 1, 3, 9, 2, 11)
        mossBox(1, 3, 1, 10, 6, 1)
        mossBox(1, 3, 13, 10, 6, 13)
        mossBox(1, 3, 2, 1, 6, 12)
        mossBox(10, 3, 2, 10, 6, 12)
        mossBox(2, 3, 2, 9, 3, 12)
        mossBox(2, 6, 2, 9, 6, 12)
        mossBox(3, 7, 3, 8, 7, 11)
        mossBox(4, 8, 4, 7, 8, 10)
        airBox(3, 1, 3, 8, 2, 11)
        airBox(4, 3, 6, 7, 3, 9)
        airBox(2, 4, 2, 9, 5, 12)
        airBox(4, 6, 5, 7, 6, 9)
        airBox(5, 7, 6, 6, 7, 8)
        airBox(5, 1, 2, 6, 2, 2)
        airBox(5, 2, 12, 6, 2, 12)
        airBox(5, 5, 1, 6, 5, 1)
        airBox(5, 5, 13, 6, 5, 13)
        set(AIR, 1, 5, 5)
        set(AIR, 10, 5, 5)
        set(AIR, 1, 5, 9)
        set(AIR, 10, 5, 9)
    }

    /** The pillars of the two long walls and the stepped roof above them. */
    private fun buildRoof() {
        for (z in 0..14 step 14) {
            for (x in listOf(2, 4, 7, 9)) mossBox(x, 4, z, x, 5, z)
        }
        mossBox(5, 6, 0, 6, 6, 0)
        for (x in 0..11 step 11) {
            for (z in 2..12 step 2) mossBox(x, 4, z, x, 5, z)
            mossBox(x, 6, 5, x, 6, 5)
            mossBox(x, 6, 9, x, 6, 9)
        }
        // Z outer, X inner: the jar writes (2,2), (9,2), (2,12), (9,12), and the selector draws in
        // that order, so swapping the loops would hand the next box a different stream position.
        for (z in listOf(2, 12)) {
            for (x in listOf(2, 9)) mossBox(x, 7, z, x, 9, z)
        }
        for (z in listOf(4, 10)) {
            for (x in listOf(4, 7)) mossBox(x, 9, z, x, 9, z)
        }
        mossBox(5, 9, 7, 6, 9, 7)
    }

    private fun buildStairs() {
        set(stairs("north"), 5, 9, 6)
        set(stairs("north"), 6, 9, 6)
        set(stairs("south"), 5, 9, 8)
        set(stairs("south"), 6, 9, 8)
        for (x in 4..7) set(stairs("north"), x, 0, 0)
        for (x in listOf(4, 7)) {
            set(stairs("north"), x, 1, 8)
            set(stairs("north"), x, 2, 9)
            set(stairs("north"), x, 3, 10)
            mossBox(x, 1, 9, x, 1, 9)
        }
        mossBox(4, 1, 10, 7, 2, 10)
        mossBox(5, 4, 5, 6, 4, 5)
        set(stairs("east"), 4, 4, 5)
        set(stairs("west"), 7, 4, 5)
        // The staircase down into the cellar, cut a step at a time.
        for (step in 0 until 4) {
            set(stairs("south"), 5, -step, 6 + step)
            set(stairs("south"), 6, -step, 6 + step)
            airBox(5, -step, 7 + step, 6, -step, 9 + step)
        }
    }

    private fun buildCellar() {
        airBox(1, -3, 12, 10, -1, 13)
        airBox(1, -3, 1, 3, -1, 13)
        airBox(1, -3, 1, 9, -1, 5)
        for (z in 1..13 step 2) mossBox(1, -3, z, 1, -2, z)
        for (z in 2..12 step 2) mossBox(1, -1, z, 3, -1, z)
        mossBox(2, -2, 1, 5, -2, 1)
        mossBox(7, -2, 1, 9, -2, 1)
        mossBox(6, -3, 1, 6, -3, 1)
        mossBox(6, -1, 1, 6, -1, 1)
    }

    /** The east-west tripwire and the dispenser it fires. */
    private fun buildTripwireTrap() {
        set(tripwireHook("east"), 1, -3, 8)
        set(tripwireHook("west"), 4, -3, 8)
        set(tripwire("east", "west"), 2, -3, 8)
        set(tripwire("east", "west"), 3, -3, 8)
        for (z in 2..7) set(REDSTONE_NORTH_SOUTH, 5, -3, z)
        set(redstone("north" to "side", "west" to "side"), 5, -3, 1)
        set(redstone("east" to "side", "west" to "side"), 4, -3, 1)
        set(MOSSY_COBBLESTONE, 3, -3, 1)
        createDispenser(3, -2, 1, "north")
        set(vine("south"), 3, -2, 2)
    }

    /** The north-south tripwire, its dispenser and the main chest behind the vines. */
    private fun buildArrowTrap() {
        set(tripwireHook("north"), 7, -3, 1)
        set(tripwireHook("south"), 7, -3, 5)
        for (z in 2..4) set(tripwire("north", "south"), 7, -3, z)
        set(redstone("east" to "side", "west" to "side"), 8, -3, 6)
        set(redstone("west" to "side", "south" to "side"), 9, -3, 6)
        set(redstone("north" to "side", "south" to "up"), 9, -3, 5)
        set(MOSSY_COBBLESTONE, 9, -3, 4)
        set(REDSTONE_NORTH_SOUTH, 9, -2, 4)
        createDispenser(9, -2, 3, "west")
        set(vine("east"), 8, -1, 3)
        set(vine("east"), 8, -2, 3)
        createChest(8, -3, 3)
        set(MOSSY_COBBLESTONE, 9, -3, 2)
        set(MOSSY_COBBLESTONE, 8, -3, 1)
        for (x in listOf(4, 6, 8)) set(MOSSY_COBBLESTONE, x, -3, 5)
        for (x in listOf(5, 7)) {
            set(MOSSY_COBBLESTONE, x, -2, 5)
            set(MOSSY_COBBLESTONE, x, -1, 5)
        }
        mossBox(9, -1, 1, 9, -1, 5)
    }

    /** The three levers, the pistons they drive and the chest they hide. */
    private fun buildLeverPuzzle() {
        airBox(8, -3, 8, 10, -1, 10)
        for (x in 8..10) {
            set(CHISELED_STONE_BRICKS, x, -2, 11)
            set(WALL_LEVER_NORTH, x, -2, 12)
        }
        mossBox(8, -3, 8, 8, -3, 10)
        mossBox(10, -3, 8, 10, -3, 10)
        set(MOSSY_COBBLESTONE, 10, -2, 9)
        set(REDSTONE_NORTH_SOUTH, 8, -2, 9)
        set(REDSTONE_NORTH_SOUTH, 8, -2, 10)
        set(redstone("north" to "side", "south" to "side", "east" to "side", "west" to "side"), 10, -1, 9)
        set(stickyPiston("up"), 9, -2, 8)
        set(stickyPiston("west"), 10, -2, 8)
        set(stickyPiston("west"), 10, -1, 8)
        set(repeater("north"), 10, -2, 10)
        createChest(9, -3, 10)
    }

    /**
     * // parity: generateBox with the MossStoneSelector. The draw happens for every position of the
     * box and the clip is applied after, so this must not skip anything.
     */
    private fun mossBox(
        x0: Int,
        y0: Int,
        z0: Int,
        x1: Int,
        y1: Int,
        z1: Int,
    ) {
        for (y in y0..y1) {
            for (x in x0..x1) {
                for (z in z0..z1) {
                    val stateId = if (random.nextFloat() < COBBLESTONE_CHANCE) COBBLESTONE else MOSSY_COBBLESTONE
                    set(stateId, x, y, z)
                }
            }
        }
    }

    private fun airBox(
        x0: Int,
        y0: Int,
        z0: Int,
        x1: Int,
        y1: Int,
        z1: Int,
    ) {
        for (y in y0..y1) {
            for (x in x0..x1) {
                for (z in z0..z1) set(AIR, x, y, z)
            }
        }
    }

    private fun createChest(
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
                mapOf("facing" to facing, "type" to "single", "waterlogged" to "false"),
            ) ?: return
        chunk.setBlockStateId(worldX and 15, worldY, worldZ and 15, stateId)
        putLoot(worldX, worldY, worldZ, "chest", "minecraft:chests/jungle_temple", seed)
    }

    private fun createDispenser(
        x: Int,
        y: Int,
        z: Int,
        facing: String,
    ) {
        val worldX = worldX(x, z)
        val worldY = worldY(y)
        val worldZ = worldZ(x, z)
        if (!clip.isInside(StructurePos(worldX, worldY, worldZ))) return
        if (Blocks.byStateId(chunk.getDecorationBlockStateId(worldX, worldY, worldZ))?.block?.path ==
            "dispenser"
        ) {
            return
        }
        val stateId =
            Blocks.stateIdOf(
                Identifier.vanilla("dispenser"),
                mapOf("facing" to transformedFacing(facing), "triggered" to "false"),
            ) ?: return
        chunk.setBlockStateId(worldX and 15, worldY, worldZ and 15, stateId)
        putLoot(worldX, worldY, worldZ, "dispenser", "minecraft:chests/jungle_temple_dispenser", random.nextLong())
    }

    private fun putLoot(
        worldX: Int,
        worldY: Int,
        worldZ: Int,
        type: String,
        table: String,
        seed: Long,
    ) {
        chunk.setBlockEntity(
            ChunkBlockEntity(
                worldX and 15,
                worldY,
                worldZ and 15,
                Identifier.vanilla(type),
                NbtCompound().apply {
                    putString("LootTable", table)
                    putLong("LootTableSeed", seed)
                    putCompound("components", NbtCompound())
                },
            ),
        )
    }

    /** // parity: StructurePiece.reorient. */
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

    /** A dispenser's facing is a plain property, so it turns with the piece like any other block. */
    private fun transformedFacing(facing: String): String {
        val stateId =
            Blocks.stateIdOf(
                Identifier.vanilla("dispenser"),
                mapOf("facing" to facing, "triggered" to "false"),
            )
        val turned = stateId?.let(::transformed) ?: return facing
        return Blocks.byStateId(turned)?.properties?.get("facing") ?: facing
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
        const val WIDTH: Int = 12
        const val DEPTH: Int = 15
        const val HEIGHT: Int = 10

        private const val LAST_X = WIDTH - 1
        private const val LAST_Z = DEPTH - 1
        private const val FLOOR_Y = 64
        private const val COBBLESTONE_CHANCE = 0.4f

        /** // parity: StructurePiece.makeBoundingBox, which swaps width and depth on the X axis. */
        fun makeBoundingBox(
            x: Int,
            y: Int,
            z: Int,
            orientation: StructureDirection,
        ): StructureBoundingBox =
            if (orientation == StructureDirection.NORTH || orientation == StructureDirection.SOUTH) {
                StructureBoundingBox(x, y, z, x + WIDTH - 1, y + HEIGHT - 1, z + DEPTH - 1)
            } else {
                StructureBoundingBox(x, y, z, x + DEPTH - 1, y + HEIGHT - 1, z + WIDTH - 1)
            }

        private val TEMPLATE = StructureTemplate()

        private fun state(path: String): Int =
            Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: Blocks.AIR.stateId

        private fun stairs(facing: String): Int =
            Blocks.stateIdOf(
                Identifier.vanilla("cobblestone_stairs"),
                mapOf("facing" to facing, "half" to "bottom", "shape" to "straight", "waterlogged" to "false"),
            ) ?: state("cobblestone_stairs")

        private fun tripwireHook(facing: String): Int =
            Blocks.stateIdOf(
                Identifier.vanilla("tripwire_hook"),
                mapOf("facing" to facing, "attached" to "true", "powered" to "false"),
            ) ?: state("tripwire_hook")

        private fun tripwire(
            first: String,
            second: String,
        ): Int {
            val properties =
                mutableMapOf(
                    "attached" to "true",
                    "disarmed" to "false",
                    "powered" to "false",
                    "north" to "false",
                    "south" to "false",
                    "east" to "false",
                    "west" to "false",
                )
            properties[first] = "true"
            properties[second] = "true"
            return Blocks.stateIdOf(Identifier.vanilla("tripwire"), properties) ?: state("tripwire")
        }

        private fun redstone(vararg sides: Pair<String, String>): Int {
            val properties =
                mutableMapOf(
                    "north" to "none",
                    "south" to "none",
                    "east" to "none",
                    "west" to "none",
                    "power" to "0",
                )
            for ((side, value) in sides) properties[side] = value
            return Blocks.stateIdOf(Identifier.vanilla("redstone_wire"), properties) ?: state("redstone_wire")
        }

        private fun vine(side: String): Int {
            val properties =
                mutableMapOf(
                    "north" to "false",
                    "south" to "false",
                    "east" to "false",
                    "west" to "false",
                    "up" to "false",
                )
            properties[side] = "true"
            return Blocks.stateIdOf(Identifier.vanilla("vine"), properties) ?: state("vine")
        }

        private fun stickyPiston(facing: String): Int =
            Blocks.stateIdOf(Identifier.vanilla("sticky_piston"), mapOf("facing" to facing, "extended" to "false"))
                ?: state("sticky_piston")

        private fun repeater(facing: String): Int =
            Blocks.stateIdOf(
                Identifier.vanilla("repeater"),
                mapOf("facing" to facing, "delay" to "1", "locked" to "false", "powered" to "false"),
            ) ?: state("repeater")

        private val AIR = Blocks.AIR.stateId
        private val COBBLESTONE = state("cobblestone")
        private val MOSSY_COBBLESTONE = state("mossy_cobblestone")
        private val CHISELED_STONE_BRICKS = state("chiseled_stone_bricks")
        private val REDSTONE_NORTH_SOUTH = redstone("north" to "side", "south" to "side")
        private val WALL_LEVER_NORTH =
            Blocks.stateIdOf(
                Identifier.vanilla("lever"),
                mapOf("facing" to "north", "face" to "wall", "powered" to "false"),
            ) ?: state("lever")

        private val FACING_OFFSETS =
            listOf("north" to (0 to -1), "east" to (1 to 0), "south" to (0 to 1), "west" to (-1 to 0))

        private val OPPOSITE = mapOf("north" to "south", "south" to "north", "east" to "west", "west" to "east")
    }
}
