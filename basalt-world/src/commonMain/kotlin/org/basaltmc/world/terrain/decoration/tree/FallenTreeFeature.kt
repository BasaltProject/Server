package org.basaltmc.world.terrain.decoration.tree

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk
import org.basaltmc.world.WorldHeight
import org.basaltmc.world.terrain.decoration.IntProvider

/** Concrete configuration for Minecraft 26.2's fallen-tree feature. */
public data class FallenTreeConfiguration(
    val logStateId: Int,
    val logLength: IntProvider,
    val stumpVines: Boolean = false,
)

/** Vanilla fallen-tree configurations used by Overworld biome selectors. */
public object FallenTreeConfigs {
    public val OAK: FallenTreeConfiguration = FallenTreeConfiguration(state("oak_log"), IntProvider.Uniform(4, 7), stumpVines = true)
    public val BIRCH: FallenTreeConfiguration = FallenTreeConfiguration(state("birch_log"), IntProvider.Uniform(5, 8))
    public val SUPER_BIRCH: FallenTreeConfiguration = FallenTreeConfiguration(state("birch_log"), IntProvider.Uniform(5, 15))
    public val SPRUCE: FallenTreeConfiguration = FallenTreeConfiguration(state("spruce_log"), IntProvider.Uniform(6, 10))
    public val JUNGLE: FallenTreeConfiguration = FallenTreeConfiguration(state("jungle_log"), IntProvider.Uniform(4, 11), stumpVines = true)

    private fun state(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")
}

/** // parity: FallenTreeFeature shape, validation, stump vines, and attached mushrooms. */
public object FallenTreeFeature {
    public fun place(
        chunk: Chunk,
        random: WorldgenRandom,
        originX: Int,
        originY: Int,
        originZ: Int,
        config: FallenTreeConfiguration,
    ): Boolean {
        set(chunk, originX, originY, originZ, config.logStateId)
        if (config.stumpVines) decorateStumpVines(chunk, random, originX, originY, originZ)
        val direction = HORIZONTAL_DIRECTIONS[random.nextInt(4)]
        val logLength = config.logLength.sample(random) - 2
        val distanceFromStump = 2 + random.nextInt(2)
        val startX = originX + direction.dx * distanceFromStump
        var startY = originY + 1
        val startZ = originZ + direction.dz * distanceFromStump
        for (probe in 0 until 6) {
            if (mayPlaceOn(chunk, startX, startY, startZ)) break
            startY--
        }

        var gap = 0
        for (index in 0 until logLength) {
            val x = startX + direction.dx * index
            val z = startZ + direction.dz * index
            if (!isValidTreePos(chunk, x, startY, z)) return true
            if (isOverSolidGround(chunk, x, startY, z)) {
                gap = 0
            } else if (++gap > 2) {
                return true
            }
        }
        val sidewaysState = config.logStateId + if (direction.axisX) -1 else 1
        val fallenLogs = ArrayList<LogPos>(logLength)
        for (index in 0 until logLength) {
            val log = LogPos(startX + direction.dx * index, startY, startZ + direction.dz * index)
            set(chunk, log.x, log.y, log.z, sidewaysState)
            fallenLogs += log
        }
        decorateAttachedMushrooms(chunk, random, fallenLogs)
        return true
    }

    private fun decorateStumpVines(
        chunk: Chunk,
        random: WorldgenRandom,
        x: Int,
        y: Int,
        z: Int,
    ) {
        for (vine in STUMP_VINES) {
            if (random.nextInt(3) > 0 && isAir(chunk, x + vine.dx, y, z + vine.dz)) {
                set(chunk, x + vine.dx, y, z + vine.dz, defaultVineStateId - vine.faceBit)
            }
        }
    }

    private fun decorateAttachedMushrooms(
        chunk: Chunk,
        random: WorldgenRandom,
        logs: List<LogPos>,
    ) {
        val shuffled = javaHashSetOrder(logs).toMutableList()
        for (size in shuffled.size downTo 2) {
            val swapTo = random.nextInt(size)
            val last = shuffled[size - 1]
            shuffled[size - 1] = shuffled[swapTo]
            shuffled[swapTo] = last
        }
        for (log in shuffled) {
            random.nextInt(1)
            if (random.nextFloat() <= ATTACHED_MUSHROOM_PROBABILITY && isAir(chunk, log.x, log.y + 1, log.z)) {
                val mushroom = if (random.nextInt(3) < 2) redMushroomStateId else brownMushroomStateId
                set(chunk, log.x, log.y + 1, log.z, mushroom)
            }
        }
    }

    private fun javaHashSetOrder(positions: List<LogPos>): Sequence<LogPos> {
        if (positions.isEmpty()) return emptySequence()
        var capacity = 16
        while (positions.size > capacity * 3 / 4) capacity *= 2
        return sequence {
            for (bucket in 0 until capacity) {
                for (position in positions) {
                    val hash = (position.y + position.z * 31) * 31 + position.x
                    val spread = hash xor (hash ushr 16)
                    if (spread and (capacity - 1) == bucket) yield(position)
                }
            }
        }
    }

    private fun mayPlaceOn(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean = isValidTreePos(chunk, x, y, z) && isOverSolidGround(chunk, x, y, z)

    private fun isOverSolidGround(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean = y > WorldHeight.MIN_Y && Chunk.HeightmapType.OCEAN_FLOOR.matches(chunk.getBlockStateId(x and 15, y - 1, z and 15))

    private fun isValidTreePos(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean = y in WorldHeight.MIN_Y..WorldHeight.MAX_Y && chunk.getBlockStateId(x and 15, y, z and 15) == Blocks.AIR.stateId

    private fun isAir(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean = y in WorldHeight.MIN_Y..WorldHeight.MAX_Y && chunk.getBlockStateId(x and 15, y, z and 15) == Blocks.AIR.stateId

    private fun set(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
    ) {
        if (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) chunk.setBlockStateId(x and 15, y, z and 15, stateId)
    }

    private data class HorizontalDirection(
        val dx: Int,
        val dz: Int,
        val axisX: Boolean,
    )

    private data class StumpVine(
        val dx: Int,
        val dz: Int,
        val faceBit: Int,
    )

    private data class LogPos(
        val x: Int,
        val y: Int,
        val z: Int,
    )

    private val HORIZONTAL_DIRECTIONS =
        listOf(
            HorizontalDirection(0, -1, false),
            HorizontalDirection(1, 0, true),
            HorizontalDirection(0, 1, false),
            HorizontalDirection(-1, 0, true),
        )

    private val STUMP_VINES =
        listOf(
            StumpVine(-1, 0, 16),
            StumpVine(1, 0, 1),
            StumpVine(0, -1, 4),
            StumpVine(0, 1, 8),
        )
    private const val ATTACHED_MUSHROOM_PROBABILITY = 0.1f
    private val defaultVineStateId = state("vine")
    private val redMushroomStateId = state("red_mushroom")
    private val brownMushroomStateId = state("brown_mushroom")

    private fun state(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")
}
