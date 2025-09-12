package org.basaltmc.server.world.structure

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.ChunkBlockEntity
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtType

/** // parity: the blocks every stronghold piece shares. */
internal object StrongholdPalette {
    fun state(path: String): Int = Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: Blocks.AIR.stateId

    fun state(
        path: String,
        properties: Map<String, String>,
    ): Int = Blocks.stateIdOf(Identifier.vanilla(path), properties) ?: state(path)

    val STONE_BRICKS: Int = state("stone_bricks")
    val CRACKED: Int = state("cracked_stone_bricks")
    val MOSSY: Int = state("mossy_stone_bricks")
    val INFESTED: Int = state("infested_stone_bricks")
    val CAVE_AIR: Int = state("cave_air")
    val SMOOTH_STONE_SLAB: Int = state("smooth_stone_slab")
    val STONE_BRICK_SLAB: Int = state("stone_brick_slab")
    val COBBLESTONE: Int = state("cobblestone")
    val IRON_DOOR_LOWER: Int = state("iron_door", door("west", "lower"))
    val IRON_DOOR_UPPER: Int = state("iron_door", door("west", "upper"))

    private fun door(
        facing: String,
        half: String,
    ) = mapOf(
        "facing" to facing,
        "half" to half,
        "hinge" to "left",
        "open" to "false",
        "powered" to "false",
    )

    fun oakDoor(
        facing: String,
        half: String,
    ): Int = state("oak_door", door(facing, half))

    fun ironDoor(
        facing: String,
        half: String,
    ): Int = state("iron_door", door(facing, half))

    fun bars(vararg sides: String): Int =
        state(
            "iron_bars",
            mapOf(
                "north" to (sides.contains("north")).toString(),
                "south" to (sides.contains("south")).toString(),
                "east" to (sides.contains("east")).toString(),
                "west" to (sides.contains("west")).toString(),
                "waterlogged" to "false",
            ),
        )

    fun stairs(
        path: String,
        facing: String,
        half: String = "bottom",
    ): Int =
        state(
            path,
            mapOf(
                "facing" to facing,
                "half" to half,
                "shape" to "straight",
                "waterlogged" to "false",
            ),
        )

    fun wallTorch(facing: String): Int = state("wall_torch", mapOf("facing" to facing))

    fun button(facing: String): Int =
        state(
            "stone_button",
            mapOf("facing" to facing, "face" to "wall", "powered" to "false"),
        )
}

/**
 * // parity: the drawing half of StrongholdPiece. Everything a room writes goes through here, so the
 * two rules that decide a stronghold's random stream live in one place: the shell skips whatever is
 * already air, and skipping it also skips the draw that would have chosen its block.
 */
internal class StrongholdBrush(
    val fill: StructurePieceFill,
    private val chunk: Chunk,
    val random: BasaltRandom,
) {
    fun place(
        stateId: Int,
        x: Int,
        y: Int,
        z: Int,
    ) {
        fill.place(stateId, x, y, z)
    }

    fun box(
        x0: Int,
        y0: Int,
        z0: Int,
        x1: Int,
        y1: Int,
        z1: Int,
        stateId: Int,
    ) {
        fill.generateBox(x0, y0, z0, x1, y1, z1, stateId, stateId, skipAir = false)
    }

    /** // parity: maybeGenerateBlock, whose draw happens whether or not the block lands. */
    fun maybe(
        chance: Float,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
    ) {
        if (random.nextFloat() < chance) fill.place(stateId, x, y, z)
    }

    /**
     * // parity: generateBox with SmoothStoneSelector. The edge is stone brick weathered four ways
     * and the inside is hollowed to cave air; with [skipAir] an air block is left alone **and draws
     * nothing**, which is why a stronghold that meets a cave has holes rather than a shifted stream.
     */
    fun shell(
        x0: Int,
        y0: Int,
        z0: Int,
        x1: Int,
        y1: Int,
        z1: Int,
        skipAir: Boolean = true,
    ) {
        for (y in y0..y1) {
            for (x in x0..x1) {
                for (z in z0..z1) {
                    if (skipAir && Blocks.isAir(fill.blockAt(x, y, z))) continue
                    val edge = y == y0 || y == y1 || x == x0 || x == x1 || z == z0 || z == z1
                    fill.place(smoothStone(edge), x, y, z)
                }
            }
        }
    }

    private fun smoothStone(isEdge: Boolean): Int {
        if (!isEdge) return StrongholdPalette.CAVE_AIR
        val selection = random.nextFloat()
        return when {
            selection < 0.2f -> StrongholdPalette.CRACKED
            selection < 0.5f -> StrongholdPalette.MOSSY
            selection < 0.55f -> StrongholdPalette.INFESTED
            else -> StrongholdPalette.STONE_BRICKS
        }
    }

    /** // parity: generateSmallDoor, the four ways a room opens onto the one before it. */
    fun door(
        type: StrongholdDoor,
        x: Int,
        y: Int,
        z: Int,
    ) {
        when (type) {
            StrongholdDoor.OPENING -> box(x, y, z, x + 2, y + 2, z, StrongholdPalette.CAVE_AIR)
            StrongholdDoor.WOOD_DOOR -> {
                frame(x, y, z)
                place(StrongholdPalette.oakDoor("north", "lower"), x + 1, y, z)
                place(StrongholdPalette.oakDoor("north", "upper"), x + 1, y + 1, z)
            }
            StrongholdDoor.GRATES -> {
                place(StrongholdPalette.CAVE_AIR, x + 1, y, z)
                place(StrongholdPalette.CAVE_AIR, x + 1, y + 1, z)
                place(StrongholdPalette.bars("west"), x, y, z)
                place(StrongholdPalette.bars("west"), x, y + 1, z)
                place(StrongholdPalette.bars("east", "west"), x, y + 2, z)
                place(StrongholdPalette.bars("east", "west"), x + 1, y + 2, z)
                place(StrongholdPalette.bars("east", "west"), x + 2, y + 2, z)
                place(StrongholdPalette.bars("east"), x + 2, y + 1, z)
                place(StrongholdPalette.bars("east"), x + 2, y, z)
            }
            StrongholdDoor.IRON_DOOR -> {
                frame(x, y, z)
                place(StrongholdPalette.ironDoor("north", "lower"), x + 1, y, z)
                place(StrongholdPalette.ironDoor("north", "upper"), x + 1, y + 1, z)
                place(StrongholdPalette.button("north"), x + 2, y + 1, z + 1)
                place(StrongholdPalette.button("south"), x + 2, y + 1, z - 1)
            }
        }
    }

    /** // parity: StructurePiece.createChest with `reorient`, which turns the chest away from its wall. */
    fun createChest(
        x: Int,
        y: Int,
        z: Int,
        lootTable: String,
    ) {
        val worldX = fill.worldX(x, z)
        val worldY = fill.worldY(y)
        val worldZ = fill.worldZ(x, z)
        if (!inClip(worldX, worldY, worldZ)) return
        if (Blocks.byStateId(chunk.getDecorationBlockStateId(worldX, worldY, worldZ))?.block?.path == "chest") return
        val seed = random.nextLong()
        val stateId =
            StrongholdPalette.state(
                "chest",
                mapOf("facing" to reorient(worldX, worldY, worldZ), "type" to "single", "waterlogged" to "false"),
            )
        chunk.setBlockStateId(worldX and 15, worldY, worldZ and 15, stateId)
        chunk.setBlockEntity(
            ChunkBlockEntity(
                worldX and 15,
                worldY,
                worldZ and 15,
                Identifier.vanilla("chest"),
                NbtCompound().apply {
                    putString("LootTable", lootTable)
                    putLong("LootTableSeed", seed)
                    putCompound("components", NbtCompound())
                },
            ),
        )
    }

    /**
     * // parity: the portal room's silverfish spawner. The jar writes it straight to the level, so it
     * skips the piece's orientation and its replacement rule, and `setEntityId` draws nothing.
     */
    fun silverfishSpawner(
        x: Int,
        y: Int,
        z: Int,
    ) {
        val worldX = fill.worldX(x, z)
        val worldY = fill.worldY(y)
        val worldZ = fill.worldZ(x, z)
        if (!inClip(worldX, worldY, worldZ)) return
        chunk.setBlockStateId(worldX and 15, worldY, worldZ and 15, StrongholdPalette.state("spawner"))
        chunk.setBlockEntity(
            ChunkBlockEntity(
                worldX and 15,
                worldY,
                worldZ and 15,
                Identifier.vanilla("mob_spawner"),
                NbtCompound().apply {
                    putString("id", "minecraft:mob_spawner")
                    putShort("Delay", 20)
                    putShort("MinSpawnDelay", 200)
                    putShort("MaxSpawnDelay", 800)
                    putShort("SpawnCount", 4)
                    putShort("MaxNearbyEntities", 6)
                    putShort("RequiredPlayerRange", 16)
                    putShort("SpawnRange", 4)
                    putCompound(
                        "SpawnData",
                        NbtCompound().apply {
                            putCompound("entity", NbtCompound().apply { putString("id", "minecraft:silverfish") })
                        },
                    )
                    this["SpawnPotentials"] = NbtList(NbtType.END)
                },
            ),
        )
    }

    private fun inClip(
        worldX: Int,
        worldY: Int,
        worldZ: Int,
    ): Boolean =
        worldX shr 4 == chunk.chunkX &&
            worldZ shr 4 == chunk.chunkZ &&
            worldY in chunk.verticalBounds.minY..chunk.verticalBounds.maxY

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

    private fun frame(
        x: Int,
        y: Int,
        z: Int,
    ) {
        val bricks = StrongholdPalette.STONE_BRICKS
        place(bricks, x, y, z)
        place(bricks, x, y + 1, z)
        place(bricks, x, y + 2, z)
        place(bricks, x + 1, y + 2, z)
        place(bricks, x + 2, y + 2, z)
        place(bricks, x + 2, y + 1, z)
        place(bricks, x + 2, y, z)
    }

    private companion object {
        val FACING_OFFSETS =
            listOf(
                "north" to (0 to -1),
                "south" to (0 to 1),
                "west" to (-1 to 0),
                "east" to (1 to 0),
            )

        val OPPOSITE = mapOf("north" to "south", "south" to "north", "west" to "east", "east" to "west")
    }
}
