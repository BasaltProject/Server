package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.BlockFace
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.ChunkBlockEntity
import org.basaltmc.server.world.ChunkScheduledTick
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.decoration.tree.TreeFeature
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtType
import kotlin.math.abs

/**
 * The features that are a shape rather than a scattered patch: springs, the dungeon, the End's own
 * platforms, lakes, and the lush cave floors.
 *
 * parity: `SpringFeature`, `MonsterRoomFeature`, `EndPlatformFeature`, `VoidStartPlatformFeature`,
 * `LakeFeature`, `VegetationPatchFeature` and `BlockColumnFeature`'s downward form, each at the
 * position it was handed. Generation reaches them through its own steps; this is the door
 * `/place feature` comes in by. Multi-block bodies write the intersection this chunk can own;
 * block entities and their worldgen data are retained beside those local blocks.
 */
public object StandaloneFeatures {
    public val PLACED_IDS: Set<String> =
        setOf(
            "spring_closed",
            "spring_closed_double",
            "sulfur_pool",
            "monster_room",
            "monster_room_deep",
            "end_platform",
            "void_start_platform",
            "end_gateway_return",
            "lake_lava_surface",
            "lake_lava_underground",
            "cave_vines",
            "classic_vines_cave_feature",
            "lush_caves_vegetation",
            "lush_caves_ceiling_vegetation",
            "lush_caves_clay",
            "rooted_azalea_tree",
        )

    public fun placeAt(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
        placedFeature: String,
    ): Boolean =
        when (placedFeature) {
            // parity: `spring_closed` is a lava spring walled in by five rocks and no hole.
            "spring_closed" -> placeSpring(chunk, origin, "netherrack", lavaStateId, rockCount = 5, holeCount = 0)
            "spring_closed_double" ->
                placeSpring(
                    chunk,
                    origin,
                    "netherrack",
                    lavaStateId,
                    rockCount = 5,
                    holeCount = 0,
                )
            "sulfur_pool" -> placeSulfurPool(chunk, random, origin)
            "monster_room", "monster_room_deep" -> placeMonsterRoom(chunk, random, origin)
            "end_platform" -> placeEndPlatform(chunk, origin)
            "void_start_platform" -> placeVoidPlatform(chunk, origin)
            "end_gateway_return" -> placeGateway(chunk, origin)
            "lake_lava_surface", "lake_lava_underground" ->
                placeLake(chunk, random, origin, fluid = lavaStateId, barrier = state("stone"))
            "cave_vines" -> CaveVineColumnFeature.place(chunk, random, origin)
            "classic_vines_cave_feature" -> placeVines(chunk, origin)
            "lush_caves_vegetation" -> LushVegetationPatchFeature.placeFloorMoss(chunk, random, origin)
            "lush_caves_ceiling_vegetation" -> LushVegetationPatchFeature.placeCeilingMoss(chunk, random, origin)
            "lush_caves_clay" -> LushVegetationPatchFeature.placeClay(chunk, random, origin)
            "rooted_azalea_tree" -> RootedAzaleaFeature.place(chunk, random, origin)
            else -> false
        }

    /** parity: `SpringFeature`, which counts the rocks and the holes around the origin before writing. */
    private fun placeSpring(
        chunk: Chunk,
        origin: DecorationPos,
        validBlock: String,
        fluidStateId: Int,
        rockCount: Int,
        holeCount: Int,
    ): Boolean {
        if (pathAt(chunk, origin.copy(y = origin.y + 1)) != validBlock) return false
        val current = stateAt(chunk, origin)
        if (!Blocks.isAir(current) && Blocks.byStateId(current)?.block?.path != validBlock) return false

        var rocks = 0
        var holes = 0
        for (side in sidesAndBelow(origin)) {
            if (pathAt(chunk, side) == validBlock) rocks++
            if (Blocks.isAir(stateAt(chunk, side))) holes++
        }
        if (rocks != rockCount || holes != holeCount) return false
        set(chunk, origin, fluidStateId)
        val fluid = Blocks.byStateId(fluidStateId)?.block ?: return true
        if (origin.x shr 4 == chunk.chunkX && origin.z shr 4 == chunk.chunkZ) {
            chunk.scheduleFluidTick(
                ChunkScheduledTick(fluid, origin.x and 15, origin.y, origin.z and 15, delay = 0),
            )
        }
        return true
    }

    /** parity: `MonsterRoomFeature`: protected carve, up to two loot chests, and a populated spawner. */
    private fun placeMonsterRoom(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
    ): Boolean {
        val radiusX = random.nextInt(2) + 2
        val radiusZ = random.nextInt(2) + 2
        val minX = -radiusX - 1
        val maxX = radiusX + 1
        val minZ = -radiusZ - 1
        val maxZ = radiusZ + 1

        var openings = 0
        for (dx in minX..maxX) {
            for (dy in -1..4) {
                for (dz in minZ..maxZ) {
                    val position = DecorationPos(origin.x + dx, origin.y + dy, origin.z + dz)
                    val solid = BlockShapes.isSolidState(stateAt(chunk, position))
                    if ((dy == -1 || dy == 4) && !solid) return false
                    val onWall = dx == minX || dx == maxX || dz == minZ || dz == maxZ
                    if (onWall &&
                        dy == 0 &&
                        Blocks.isAir(stateAt(chunk, position)) &&
                        Blocks.isAir(stateAt(chunk, position.copy(y = position.y + 1)))
                    ) {
                        openings++
                    }
                }
            }
        }
        if (openings !in 1..5) return false

        for (dx in minX..maxX) {
            for (dy in 3 downTo -1) {
                for (dz in minZ..maxZ) {
                    val position = DecorationPos(origin.x + dx, origin.y + dy, origin.z + dz)
                    val onShell = dx == minX || dy == -1 || dz == minZ || dx == maxX || dy == 4 || dz == maxZ
                    if (!onShell) {
                        if (path(stateAt(chunk, position)) !in setOf("chest", "spawner")) {
                            safeSetFeature(chunk, position, Blocks.CAVE_AIR.stateId)
                        }
                        continue
                    }
                    val below = position.copy(y = position.y - 1)
                    val wallStateId = stateAt(chunk, position)
                    if (position.y >= WorldHeight.MIN_Y && !BlockShapes.isSolidState(stateAt(chunk, below))) {
                        set(chunk, position, Blocks.CAVE_AIR.stateId)
                    } else if (BlockShapes.isSolidState(wallStateId) && path(wallStateId) != "chest") {
                        val mossy = dy == -1 && random.nextInt(4) != 0
                        safeSetFeature(
                            chunk,
                            position,
                            if (mossy) state("mossy_cobblestone") else state("cobblestone"),
                        )
                    }
                }
            }
        }

        repeat(2) {
            for (attempt in 0 until 3) {
                val chestPosition =
                    DecorationPos(
                        origin.x + random.nextInt(radiusX * 2 + 1) - radiusX,
                        origin.y,
                        origin.z + random.nextInt(radiusZ * 2 + 1) - radiusZ,
                    )
                if (!Blocks.isAir(stateAt(chunk, chestPosition))) continue
                val solidNeighbours =
                    HORIZONTAL_OFFSETS.count { (dx, dz) ->
                        BlockShapes.isSolidState(stateAt(chunk, chestPosition.offset(dx, 0, dz)))
                    }
                if (solidNeighbours == 1 && placeDungeonChest(chunk, random, chestPosition)) break
            }
        }

        safeSetFeature(chunk, origin, state("spawner"))
        if (pathAt(chunk, origin) == "spawner") placeDungeonSpawner(chunk, random, origin)
        return true
    }

    private fun placeDungeonChest(
        chunk: Chunk,
        random: WorldgenRandom,
        position: DecorationPos,
    ): Boolean {
        if (position.x shr 4 != chunk.chunkX || position.z shr 4 != chunk.chunkZ) return false
        val facing = reorientDungeonChest(chunk, position)
        val chestStateId =
            state(
                "chest",
                linkedMapOf("facing" to facing, "type" to "single", "waterlogged" to "false"),
            )
        if (!safeSetFeature(chunk, position, chestStateId)) return false
        val data =
            NbtCompound().apply {
                putString("id", "minecraft:chest")
                putString("LootTable", "minecraft:chests/simple_dungeon")
                putLong("LootTableSeed", random.nextLong())
                putCompound("components", NbtCompound())
            }
        chunk.setBlockEntity(
            ChunkBlockEntity(position.x and 15, position.y, position.z and 15, Identifier.vanilla("chest"), data),
        )
        return true
    }

    /** parity: `StructurePiece.reorient`, including its adjacent-chest and multi-wall fallbacks. */
    private fun reorientDungeonChest(
        chunk: Chunk,
        position: DecorationPos,
    ): String {
        var solidDirection: HorizontalDirection? = null
        for (direction in HORIZONTAL_DIRECTIONS) {
            val neighbour = stateAt(chunk, position.offset(direction.dx, 0, direction.dz))
            if (path(neighbour) == "chest") return "north"
            if (!BlockShapes.isSolidRender(neighbour)) continue
            if (solidDirection != null) {
                solidDirection = null
                break
            }
            solidDirection = direction
        }
        if (solidDirection != null) return solidDirection.opposite

        var facing = HORIZONTAL_DIRECTIONS.first()
        if (isSolidRender(chunk, position, facing)) facing = direction(facing.opposite)
        if (isSolidRender(chunk, position, facing)) facing = direction(facing.clockwise)
        if (isSolidRender(chunk, position, facing)) facing = direction(facing.opposite)
        return facing.name
    }

    private fun isSolidRender(
        chunk: Chunk,
        position: DecorationPos,
        direction: HorizontalDirection,
    ): Boolean = BlockShapes.isSolidRender(stateAt(chunk, position.offset(direction.dx, 0, direction.dz)))

    private fun direction(name: String): HorizontalDirection = HORIZONTAL_DIRECTIONS.first { it.name == name }

    private fun placeDungeonSpawner(
        chunk: Chunk,
        random: WorldgenRandom,
        position: DecorationPos,
    ) {
        if (position.x shr 4 != chunk.chunkX || position.z shr 4 != chunk.chunkZ) return
        val mob = DUNGEON_MOBS[random.nextInt(DUNGEON_MOBS.size)]
        val spawnData = NbtCompound().apply { putCompound("entity", NbtCompound().apply { putString("id", mob) }) }
        val data =
            NbtCompound().apply {
                putString("id", "minecraft:mob_spawner")
                putShort("Delay", 20)
                putShort("MinSpawnDelay", 200)
                putShort("MaxSpawnDelay", 800)
                putShort("SpawnCount", 4)
                putShort("MaxNearbyEntities", 6)
                putShort("RequiredPlayerRange", 16)
                putShort("SpawnRange", 4)
                putCompound("SpawnData", spawnData)
                this["SpawnPotentials"] = NbtList(NbtType.END)
                putCompound("components", NbtCompound())
            }
        chunk.setBlockEntity(
            ChunkBlockEntity(position.x and 15, position.y, position.z and 15, Identifier.vanilla("mob_spawner"), data),
        )
    }

    private fun safeSetFeature(
        chunk: Chunk,
        position: DecorationPos,
        stateId: Int,
    ): Boolean {
        if (path(stateAt(chunk, position)) in FEATURES_CANNOT_REPLACE) return false
        set(chunk, position, stateId)
        return position.x shr 4 == chunk.chunkX && position.z shr 4 == chunk.chunkZ
    }

    /** parity: `EndPlatformFeature.createEndPlatform`: a 5x5 obsidian slab with three air layers over it. */
    private fun placeEndPlatform(
        chunk: Chunk,
        origin: DecorationPos,
    ): Boolean {
        for (dz in -2..2) {
            for (dx in -2..2) {
                for (dy in -1 until 3) {
                    val position = DecorationPos(origin.x + dx, origin.y + dy, origin.z + dz)
                    set(chunk, position, if (dy == -1) state("obsidian") else Blocks.AIR.stateId)
                }
            }
        }
        return true
    }

    /** parity: `VoidStartPlatformFeature`: fixed (8,8) centre, three blocks above the supplied Y. */
    private fun placeVoidPlatform(
        chunk: Chunk,
        origin: DecorationPos,
    ): Boolean {
        val originChunkX = origin.x shr 4
        val originChunkZ = origin.z shr 4
        if (checkerboardDistance(originChunkX, originChunkZ, PLATFORM_CHUNK_X, PLATFORM_CHUNK_Z) > 1) return true
        val platformY = origin.y + PLATFORM_Y_OFFSET
        for (z in chunk.chunkZ * 16..chunk.chunkZ * 16 + 15) {
            for (x in chunk.chunkX * 16..chunk.chunkX * 16 + 15) {
                if (checkerboardDistance(PLATFORM_X, PLATFORM_Z, x, z) > PLATFORM_RADIUS) continue
                val position = DecorationPos(x, platformY, z)
                val centre = x == PLATFORM_X && z == PLATFORM_Z
                set(chunk, position, if (centre) state("cobblestone") else state("stone"))
            }
        }
        return true
    }

    /** parity: `EndGatewayFeature`'s sparse 3x5x3 bedrock cross and exact return target. */
    private fun placeGateway(
        chunk: Chunk,
        origin: DecorationPos,
    ): Boolean {
        val bedrock = state("bedrock")
        for (dy in -2..2) {
            for (dx in -1..1) {
                for (dz in -1..1) {
                    val position = DecorationPos(origin.x + dx, origin.y + dy, origin.z + dz)
                    val sameX = dx == 0
                    val sameY = dy == 0
                    val sameZ = dz == 0
                    val verticalTip = abs(dy) == 2
                    val stateId =
                        when {
                            sameX && sameY && sameZ -> state("end_gateway")
                            sameY -> Blocks.AIR.stateId
                            verticalTip && sameX && sameZ -> bedrock
                            (sameX || sameZ) && !verticalTip -> bedrock
                            else -> Blocks.AIR.stateId
                        }
                    set(chunk, position, stateId)
                }
            }
        }
        if (origin.x shr 4 == chunk.chunkX && origin.z shr 4 == chunk.chunkZ) {
            val data =
                NbtCompound().apply {
                    putString("id", "minecraft:end_gateway")
                    putLong("Age", 0L)
                    putIntArray("exit_portal", intArrayOf(100, 50, 0))
                    putBoolean("ExactTeleport", true)
                    putCompound("components", NbtCompound())
                }
            chunk.setBlockEntity(
                ChunkBlockEntity(origin.x and 15, origin.y, origin.z and 15, Identifier.vanilla("end_gateway"), data),
            )
        }
        return true
    }

    /**
     * parity: `LakeFeature`: a union of four to seven ellipsoids in its fixed 16x8 mask, including
     * the boundary preflight, cave-air/fluid carve, and probabilistic solid barrier.
     *
     * The two configurations represented here use constant providers. Their replacement
     * predicates are the real `features_cannot_replace` and `lava_pool_stone_cannot_replace`
     * tags; [forbiddenBoundaryBlock] is sulfur_pool's additional `not sulfur_spike` predicate.
     */
    private fun placeLake(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
        fluid: Int,
        barrier: Int,
        forbiddenBoundaryBlock: String? = null,
    ): Boolean {
        if (origin.y <= WorldHeight.MIN_Y + 4) return false
        val base = origin.offset(-8, -4, -8)
        val grid = BooleanArray(LAKE_GRID_SIZE)
        repeat(random.nextInt(4) + 4) {
            val radiusX = random.nextDouble() * 6.0 + 3.0
            val radiusY = random.nextDouble() * 4.0 + 2.0
            val radiusZ = random.nextDouble() * 6.0 + 3.0
            val centreX = random.nextDouble() * (16.0 - radiusX - 2.0) + 1.0 + radiusX / 2.0
            val centreY = random.nextDouble() * (8.0 - radiusY - 4.0) + 2.0 + radiusY / 2.0
            val centreZ = random.nextDouble() * (16.0 - radiusZ - 2.0) + 1.0 + radiusZ / 2.0
            for (x in 1 until 15) {
                for (z in 1 until 15) {
                    for (y in 1 until 7) {
                        val dx = (x - centreX) / (radiusX / 2.0)
                        val dy = (y - centreY) / (radiusY / 2.0)
                        val dz = (z - centreZ) / (radiusZ / 2.0)
                        if (dx * dx + dy * dy + dz * dz < 1.0) grid[lakeIndex(x, y, z)] = true
                    }
                }
            }
        }

        forEachLakeCell { x, y, z ->
            if (!isLakeBoundary(grid, x, y, z)) return@forEachLakeCell
            val stateId = stateAt(chunk, base.offset(x, y, z))
            if (y >= LAKE_FLUID_HEIGHT && isLiquidBlock(stateId)) return false
            if (y < LAKE_FLUID_HEIGHT && !BlockShapes.isSolidState(stateId) && stateId != fluid) return false
            if (forbiddenBoundaryBlock != null && path(stateId) == forbiddenBoundaryBlock) return false
        }

        forEachLakeCell { x, y, z ->
            if (!grid[lakeIndex(x, y, z)]) return@forEachLakeCell
            val position = base.offset(x, y, z)
            if (canReplaceLakeBody(stateAt(chunk, position))) {
                set(chunk, position, if (y >= LAKE_FLUID_HEIGHT) Blocks.CAVE_AIR.stateId else fluid)
            }
        }

        if (!Blocks.isAir(barrier)) {
            forEachLakeCell { x, y, z ->
                if (!isLakeBoundary(grid, x, y, z) || (y >= LAKE_FLUID_HEIGHT && random.nextInt(2) == 0)) {
                    return@forEachLakeCell
                }
                val position = base.offset(x, y, z)
                val stateId = stateAt(chunk, position)
                if (BlockShapes.isSolidState(stateId) && canReplaceLakeBarrier(stateId)) set(chunk, position, barrier)
            }
        }

        // `lake_lava` never enters LakeFeature's water-freeze branch. Sulfur caves are temperature
        // 0.8, so sulfur_pool does not freeze either in its actual biome placement.
        return true
    }

    /** parity: sulfur_pool's `SequenceFeature`: water lake, then the four-step downward wet vent. */
    private fun placeSulfurPool(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
    ): Boolean {
        if (!placeLake(chunk, random, origin, waterStateId, state("sulfur"), "sulfur_spike")) return false
        var position = origin
        repeat(SULFUR_VENT_SCAN_STEPS + 1) { step ->
            if (BlockShapes.isSolidState(stateAt(chunk, position)) &&
                hasSourceWaterFluid(stateAt(chunk, position.copy(y = position.y + 1)))
            ) {
                set(chunk, position, wetPotentSulfurStateId)
                return true
            }
            if (step < SULFUR_VENT_SCAN_STEPS) {
                position = position.copy(y = position.y - 1)
                if (position.y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y) return false
            }
        }
        return false
    }

    private inline fun forEachLakeCell(block: (x: Int, y: Int, z: Int) -> Unit) {
        for (x in 0 until LAKE_WIDTH) {
            for (z in 0 until LAKE_WIDTH) {
                for (y in 0 until LAKE_HEIGHT) block(x, y, z)
            }
        }
    }

    private fun isLakeBoundary(
        grid: BooleanArray,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean {
        val touchesCarve =
            (x < LAKE_WIDTH - 1 && grid[lakeIndex(x + 1, y, z)]) ||
                (x > 0 && grid[lakeIndex(x - 1, y, z)]) ||
                (z < LAKE_WIDTH - 1 && grid[lakeIndex(x, y, z + 1)]) ||
                (z > 0 && grid[lakeIndex(x, y, z - 1)]) ||
                (y < LAKE_HEIGHT - 1 && grid[lakeIndex(x, y + 1, z)]) ||
                (y > 0 && grid[lakeIndex(x, y - 1, z)])
        return !grid[lakeIndex(x, y, z)] && touchesCarve
    }

    private fun lakeIndex(
        x: Int,
        y: Int,
        z: Int,
    ): Int = (x * LAKE_WIDTH + z) * LAKE_HEIGHT + y

    private fun canReplaceLakeBody(stateId: Int): Boolean = path(stateId) !in FEATURES_CANNOT_REPLACE

    private fun canReplaceLakeBarrier(stateId: Int): Boolean =
        canReplaceLakeBody(stateId) && !Blocks.isLeavesState(stateId) && !TreeFeature.isLogState(stateId)

    private fun isLiquidBlock(stateId: Int): Boolean = path(stateId) in LIQUID_BLOCK_PATHS

    private fun hasSourceWaterFluid(stateId: Int): Boolean =
        Blocks.hasWaterFluid(stateId) && stateId !in Blocks.WATER.stateId + 1..Blocks.WATER.stateId + 15

    /** parity: `BlockColumnFeature` downward: a vine of body blocks with a tip at the bottom. */
    private fun placeHangingColumn(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
        tip: Int,
        body: Int,
    ): Boolean {
        if (!Blocks.isAir(stateAt(chunk, origin))) return false
        if (!isSolid(chunk, origin.copy(y = origin.y + 1))) return false
        val length = 1 + random.nextInt(VINE_MAX_LENGTH)
        var placed = 0
        while (placed < length) {
            val position = origin.copy(y = origin.y - placed)
            if (position.y < WorldHeight.MIN_Y || !Blocks.isAir(stateAt(chunk, position))) break
            set(chunk, position, if (placed == length - 1) tip else body)
            placed++
        }
        return placed > 0
    }

    /** parity: `VinesFeature`, which hangs a vine off whatever solid face it finds. */
    private fun placeVines(
        chunk: Chunk,
        origin: DecorationPos,
    ): Boolean {
        if (!Blocks.isAir(stateAt(chunk, origin))) return false
        for (face in VINE_FACES) {
            val support = origin.offset(face.dx, face.dy, face.dz)
            val supportStateId = stateAt(chunk, support)
            if (!BlockShapes.isFaceSturdy(supportStateId, face.supportFace) &&
                !BlockShapes.isCollisionFaceFull(supportStateId, face.supportFace)
            ) {
                continue
            }
            val stateId = Blocks.stateIdOf(Identifier.vanilla("vine"), mapOf(face.property to "true")) ?: continue
            set(chunk, origin, stateId)
            return true
        }
        return false
    }

    private fun sidesAndBelow(origin: DecorationPos): List<DecorationPos> =
        listOf(
            origin.copy(x = origin.x - 1),
            origin.copy(x = origin.x + 1),
            origin.copy(z = origin.z - 1),
            origin.copy(z = origin.z + 1),
            origin.copy(y = origin.y - 1),
        )

    private fun checkerboardDistance(
        x1: Int,
        z1: Int,
        x2: Int,
        z2: Int,
    ): Int = maxOf(abs(x1 - x2), abs(z1 - z2))

    private fun isSolid(
        chunk: Chunk,
        position: DecorationPos,
    ): Boolean = Chunk.HeightmapType.OCEAN_FLOOR.matches(stateAt(chunk, position))

    private fun pathAt(
        chunk: Chunk,
        position: DecorationPos,
    ): String? = path(stateAt(chunk, position))

    private fun path(stateId: Int): String? = Blocks.byStateId(stateId)?.block?.path

    private fun stateAt(
        chunk: Chunk,
        position: DecorationPos,
    ): Int {
        if (position.y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y) return Blocks.AIR.stateId
        if (position.x shr 4 != chunk.chunkX || position.z shr 4 != chunk.chunkZ) return Blocks.AIR.stateId
        return chunk.getBlockStateId(position.x and 15, position.y, position.z and 15)
    }

    private fun set(
        chunk: Chunk,
        position: DecorationPos,
        stateId: Int,
    ) {
        if (position.y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y) return
        if (position.x shr 4 != chunk.chunkX || position.z shr 4 != chunk.chunkZ) return
        chunk.setBlockStateId(position.x and 15, position.y, position.z and 15, stateId)
    }

    private fun state(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: Blocks.AIR.stateId

    private fun state(
        path: String,
        properties: Map<String, String>,
    ): Int = Blocks.stateIdOf(Identifier.vanilla(path), properties) ?: error("unknown state '$path$properties'")

    private val lavaStateId = state("lava")
    private val waterStateId = state("water")
    private val wetPotentSulfurStateId = state("potent_sulfur", mapOf("potent_sulfur_state" to "wet"))

    private val VINE_FACES =
        listOf(
            VineFace("up", 0, 1, 0, BlockFace.DOWN),
            VineFace("north", 0, 0, -1, BlockFace.SOUTH),
            VineFace("south", 0, 0, 1, BlockFace.NORTH),
            VineFace("west", -1, 0, 0, BlockFace.EAST),
            VineFace("east", 1, 0, 0, BlockFace.WEST),
        )

    private data class VineFace(
        val property: String,
        val dx: Int,
        val dy: Int,
        val dz: Int,
        val supportFace: BlockFace,
    )

    private const val PLATFORM_RADIUS = 16
    private const val PLATFORM_X = 8
    private const val PLATFORM_Z = 8
    private const val PLATFORM_Y_OFFSET = 3
    private const val PLATFORM_CHUNK_X = 0
    private const val PLATFORM_CHUNK_Z = 0
    private const val LAKE_WIDTH = 16
    private const val LAKE_HEIGHT = 8
    private const val LAKE_FLUID_HEIGHT = 4
    private const val LAKE_GRID_SIZE = LAKE_WIDTH * LAKE_WIDTH * LAKE_HEIGHT
    private const val SULFUR_VENT_SCAN_STEPS = 4
    private const val VINE_MAX_LENGTH = 8
    private val LIQUID_BLOCK_PATHS = setOf("water", "lava", "bubble_column")
    private val FEATURES_CANNOT_REPLACE =
        setOf("bedrock", "spawner", "chest", "end_portal_frame", "reinforced_deepslate", "trial_spawner", "vault")
    private val HORIZONTAL_OFFSETS = listOf(0 to -1, 0 to 1, -1 to 0, 1 to 0)
    private val HORIZONTAL_DIRECTIONS =
        listOf(
            HorizontalDirection("north", 0, -1, "south", "east"),
            HorizontalDirection("east", 1, 0, "west", "south"),
            HorizontalDirection("south", 0, 1, "north", "west"),
            HorizontalDirection("west", -1, 0, "east", "north"),
        )
    private val DUNGEON_MOBS = listOf("minecraft:skeleton", "minecraft:zombie", "minecraft:zombie", "minecraft:spider")

    private data class HorizontalDirection(
        val name: String,
        val dx: Int,
        val dz: Int,
        val opposite: String,
        val clockwise: String,
    )
}
