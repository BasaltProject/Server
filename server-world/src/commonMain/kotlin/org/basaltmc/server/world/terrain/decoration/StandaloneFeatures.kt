package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.wire.common.Identifier
import kotlin.math.abs

/**
 * The features that are a shape rather than a scattered patch: springs, the dungeon, the End's own
 * platforms, lakes, and the lush cave floors.
 *
 * parity: `SpringFeature`, `MonsterRoomFeature`, `EndPlatformFeature`, `VoidStartPlatformFeature`,
 * `LakeFeature`, `VegetationPatchFeature` and `BlockColumnFeature`'s downward form, each at the
 * position it was handed. Generation reaches them through its own steps; this is the door
 * `/place feature` comes in by, and the parts that need a world beyond the chunk (a spawner's mob,
 * a chest's loot table) are written as far as a chunk can hold them and no further.
 */
public object StandaloneFeatures {
    public val PLACED_IDS: Set<String> =
        setOf(
            "spring_closed",
            "spring_closed_double",
            "sulfur_pool",
            "rooted_sulfur_spring",
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
            "sulfur_pool" -> placeLake(chunk, random, origin, fill = lavaStateId, barrier = state("sulfur"))
            "rooted_sulfur_spring" -> placeSpring(chunk, origin, "stone", lavaStateId, rockCount = 4, holeCount = 1)
            "monster_room", "monster_room_deep" -> placeMonsterRoom(chunk, random, origin)
            "end_platform" -> placeEndPlatform(chunk, origin)
            "void_start_platform" -> placeVoidPlatform(chunk, origin)
            "end_gateway_return" -> placeGateway(chunk, origin)
            "lake_lava_surface", "lake_lava_underground" ->
                placeLake(chunk, random, origin, fill = lavaStateId, barrier = state("stone"))
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
        return true
    }

    /** parity: `MonsterRoomFeature`: a hollow cobblestone box with a spawner, dug only where it fits. */
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
                    val solid = isSolid(chunk, position)
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
                        set(chunk, position, Blocks.AIR.stateId)
                        continue
                    }
                    if (!isSolid(chunk, position.copy(y = position.y - 1))) {
                        set(chunk, position, Blocks.AIR.stateId)
                    } else if (isSolid(chunk, position)) {
                        val mossy = dy == -1 && random.nextInt(4) != 0
                        set(chunk, position, if (mossy) state("mossy_cobblestone") else state("cobblestone"))
                    }
                }
            }
        }
        // The spawner's mob and the chests' loot tables need a world this feature is not given, so
        // the room is built and the spawner left empty rather than filled with a guess.
        set(chunk, origin, state("spawner"))
        return true
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

    /** parity: `VoidStartPlatformFeature`: stone out to sixteen blocks, one cobblestone at the centre. */
    private fun placeVoidPlatform(
        chunk: Chunk,
        origin: DecorationPos,
    ): Boolean {
        for (z in chunk.chunkZ * 16..chunk.chunkZ * 16 + 15) {
            for (x in chunk.chunkX * 16..chunk.chunkX * 16 + 15) {
                if (checkerboardDistance(origin.x, origin.z, x, z) > PLATFORM_RADIUS) continue
                val position = DecorationPos(x, origin.y, z)
                val centre = x == origin.x && z == origin.z
                set(chunk, position, if (centre) state("cobblestone") else state("stone"))
            }
        }
        return true
    }

    /** parity: `EndGatewayFeature`'s own bedrock frame, without the block entity that aims it. */
    private fun placeGateway(
        chunk: Chunk,
        origin: DecorationPos,
    ): Boolean {
        val bedrock = state("bedrock")
        for (dy in -1..2) {
            for (dx in -1..1) {
                for (dz in -1..1) {
                    val edge = dx != 0 || dz != 0
                    val position = DecorationPos(origin.x + dx, origin.y + dy, origin.z + dz)
                    if (edge) set(chunk, position, bedrock) else set(chunk, position, Blocks.AIR.stateId)
                }
            }
        }
        set(chunk, origin, state("end_gateway"))
        return true
    }

    /** parity: `LakeFeature`, narrowed to the shape: a filled bowl with a barrier shell under it. */
    private fun placeLake(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
        fill: Int,
        barrier: Int,
    ): Boolean {
        val radius = 3 + random.nextInt(3)
        var placed = false
        for (dx in -radius..radius) {
            for (dz in -radius..radius) {
                for (dy in -radius..1) {
                    val distance = dx * dx + dy * dy * LAKE_FLATTENING + dz * dz
                    if (distance > radius * radius) continue
                    val position = DecorationPos(origin.x + dx, origin.y + dy, origin.z + dz)
                    if (position.y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y) continue
                    val shell = distance > (radius - 1) * (radius - 1)
                    set(chunk, position, if (shell) barrier else fill)
                    placed = true
                }
            }
        }
        return placed
    }

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
        for ((facing, offset) in VINE_FACES) {
            val support = DecorationPos(origin.x + offset.first, origin.y, origin.z + offset.second)
            if (!isSolid(chunk, support)) continue
            val stateId = Blocks.stateIdOf(Identifier.vanilla("vine"), mapOf(facing to "true")) ?: continue
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
    ): String? = Blocks.byStateId(stateAt(chunk, position))?.block?.path

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

    private val lavaStateId = state("lava")

    private val VINE_FACES =
        listOf(
            "north" to (0 to -1),
            "south" to (0 to 1),
            "west" to (-1 to 0),
            "east" to (1 to 0),
        )

    private const val PLATFORM_RADIUS = 16
    private const val LAKE_FLATTENING = 2
    private const val VINE_MAX_LENGTH = 8
}
