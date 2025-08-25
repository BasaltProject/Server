package org.basaltmc.server.world.terrain.decoration.nether

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.terrain.NetherBiomeLookup
import org.basaltmc.wire.common.Identifier
import kotlin.math.abs
import kotlin.math.min

/** // parity: the three Basalt Deltas placed features in SURFACE_STRUCTURES. */
public object NetherSurfaceStructuresDecoration {
    public fun decorate(
        chunk: Chunk,
        seed: Long,
        biomeSource: NetherBiomeLookup,
    ) {
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        for (globalIndex in 0..2) {
            random.setFeatureSeed(decorationSeed, globalIndex, SURFACE_STRUCTURES_STEP)
            placeFeature(chunk, random, biomeSource, globalIndex)
        }
    }

    internal fun decorateFeature(
        chunk: Chunk,
        seed: Long,
        biomeSource: NetherBiomeLookup,
        globalIndex: Int,
    ) {
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        random.setFeatureSeed(decorationSeed, globalIndex, SURFACE_STRUCTURES_STEP)
        placeFeature(chunk, random, biomeSource, globalIndex)
    }

    private fun placeFeature(
        chunk: Chunk,
        random: BasaltRandom,
        biomeSource: NetherBiomeLookup,
        globalIndex: Int,
    ) {
        val count =
            if (globalIndex == 0) {
                40
            } else if (globalIndex == 1) {
                4
            } else {
                2
            }
        val origins = positionsOnEveryLayer(chunk, random, count)
        for (origin in origins) {
            if (biomeSource.blockBiomeAt(origin.x, origin.y, origin.z).id.path != "basalt_deltas") continue
            when (globalIndex) {
                0 -> placeDelta(chunk, random, origin)
                1 -> placeColumns(chunk, random, origin, ColumnConfig.SMALL)
                2 -> placeColumns(chunk, random, origin, ColumnConfig.LARGE)
            }
        }
    }

    /** The vanilla placed feature names this step draws. */
    public val PLACED_IDS: Set<String> = setOf("delta", "small_basalt_columns", "large_basalt_columns")

    /** One of them at one position, which is what `/place feature` runs. */
    public fun placeAt(
        chunk: Chunk,
        random: BasaltRandom,
        x: Int,
        y: Int,
        z: Int,
        placedFeature: String,
    ): Boolean {
        val origin = Pos(x, y, z)
        when (placedFeature) {
            "delta" -> placeDelta(chunk, random, origin)
            "small_basalt_columns" -> placeColumns(chunk, random, origin, ColumnConfig.SMALL)
            "large_basalt_columns" -> placeColumns(chunk, random, origin, ColumnConfig.LARGE)
            else -> return false
        }
        return true
    }

    private fun positionsOnEveryLayer(
        chunk: Chunk,
        random: BasaltRandom,
        count: Int,
    ): List<Pos> {
        val positions = mutableListOf<Pos>()
        var layer = 0
        do {
            var foundAny = false
            repeat(count) {
                val x = chunk.chunkX * 16 + random.nextInt(16)
                val z = chunk.chunkZ * 16 + random.nextInt(16)
                val startY = chunk.heightAt(x and 15, z and 15, Chunk.HeightmapType.MOTION_BLOCKING)
                val y = findGroundLayer(chunk, x, startY, z, layer)
                if (y != null) {
                    positions += Pos(x, y, z)
                    foundAny = true
                }
            }
            layer++
        } while (foundAny)
        return positions
    }

    private fun findGroundLayer(
        chunk: Chunk,
        x: Int,
        startY: Int,
        z: Int,
        targetLayer: Int,
    ): Int? {
        var currentLayer = 0
        var currentState = stateAt(chunk, x, startY, z)
        for (y in startY downTo chunk.verticalBounds.minY + 1) {
            val below = stateAt(chunk, x, y - 1, z)
            val surface = below != null && !isEmpty(below) && currentState != null && isEmpty(currentState)
            if (surface && blockPath(below) != "bedrock") {
                if (currentLayer == targetLayer) return y
                currentLayer++
            }
            currentState = below
        }
        return null
    }

    private fun placeDelta(
        chunk: Chunk,
        random: BasaltRandom,
        origin: Pos,
    ) {
        val spawnRim = random.nextDouble() < 0.9
        val rimX = if (spawnRim) random.nextInt(3) else 0
        val rimZ = if (spawnRim) random.nextInt(3) else 0
        val hasRim = spawnRim && rimX != 0 && rimZ != 0
        val radiusX = 3 + random.nextInt(5)
        val radiusZ = 3 + random.nextInt(5)
        val radiusLimit = maxOf(radiusX, radiusZ)
        val magma = stateId("magma_block")
        val lava = Blocks.LAVA.stateId

        visitManhattan(radiusX, radiusZ, radiusLimit) { offsetX, offsetZ ->
            val x = origin.x + offsetX
            val z = origin.z + offsetZ
            if (isDeltaClear(chunk, x, origin.y, z)) {
                if (hasRim) setBlock(chunk, x, origin.y, z, magma)
                val contentX = x + rimX
                val contentZ = z + rimZ
                if (isDeltaClear(chunk, contentX, origin.y, contentZ)) {
                    setBlock(chunk, contentX, origin.y, contentZ, lava)
                }
            }
        }
    }

    private inline fun visitManhattan(
        reachX: Int,
        reachZ: Int,
        radiusLimit: Int,
        visit: (Int, Int) -> Unit,
    ) {
        for (depth in 0..radiusLimit) {
            val maxX = min(reachX, depth)
            for (x in -maxX..maxX) {
                val z = depth - abs(x)
                if (z > reachZ) continue
                visit(x, z)
                if (z != 0) visit(x, -z)
            }
        }
    }

    private fun isDeltaClear(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean {
        val current = stateAt(chunk, x, y, z) ?: return false
        val path = blockPath(current) ?: return false
        if (path == "lava" || path in DELTA_CANNOT_REPLACE) return false
        if (!isAir(chunk, x, y + 1, z)) return false
        return isKnownNonAir(chunk, x, y - 1, z) &&
            isKnownNonAir(chunk, x, y, z - 1) &&
            isKnownNonAir(chunk, x, y, z + 1) &&
            isKnownNonAir(chunk, x - 1, y, z) &&
            isKnownNonAir(chunk, x + 1, y, z)
    }

    private fun placeColumns(
        chunk: Chunk,
        random: BasaltRandom,
        origin: Pos,
        config: ColumnConfig,
    ) {
        if (!canPlaceColumnAt(chunk, origin.x, origin.y, origin.z)) return
        val columnHeight = config.minHeight + random.nextInt(config.maxHeight - config.minHeight + 1)
        val clustered = random.nextFloat() < 0.9f
        val reach = min(columnHeight, if (clustered) 5 else 8)
        val count = if (clustered) 50 else 15
        repeat(count) {
            val x = origin.x - reach + random.nextInt(reach * 2 + 1)
            val y = origin.y + random.nextInt(1)
            val z = origin.z - reach + random.nextInt(reach * 2 + 1)
            val distance = abs(x - origin.x) + abs(y - origin.y) + abs(z - origin.z)
            val blocksToPlace = columnHeight - distance
            if (blocksToPlace >= 0) {
                val columnReach = config.minReach + random.nextInt(config.maxReach - config.minReach + 1)
                placeColumn(chunk, x, y, z, blocksToPlace, columnReach)
            }
        }
    }

    private fun placeColumn(
        chunk: Chunk,
        originX: Int,
        originY: Int,
        originZ: Int,
        columnHeight: Int,
        reach: Int,
    ) {
        val basalt = stateId("basalt")
        for (offsetZ in -reach..reach) {
            for (offsetX in -reach..reach) {
                val x = originX + offsetX
                val z = originZ + offsetZ
                val stepLimit = abs(offsetX) + abs(offsetZ)
                val startY =
                    if (isAirOrLavaOcean(chunk, x, originY, z)) {
                        findSurface(chunk, x, originY, z, stepLimit)
                    } else {
                        findAir(chunk, x, originY, z, stepLimit)
                    } ?: continue
                var blocksY = columnHeight - stepLimit / 2
                var y = startY
                while (blocksY >= 0) {
                    if (isAirOrLavaOcean(chunk, x, y, z)) {
                        setBlock(chunk, x, y, z, basalt)
                    } else if (blockPathAt(chunk, x, y, z) != "basalt") {
                        break
                    }
                    y++
                    blocksY--
                }
            }
        }
    }

    private fun findSurface(
        chunk: Chunk,
        x: Int,
        startY: Int,
        z: Int,
        initialLimit: Int,
    ): Int? {
        var y = startY
        var limit = initialLimit
        while (y > chunk.verticalBounds.minY + 1 && limit > 0) {
            limit--
            if (canPlaceColumnAt(chunk, x, y, z)) return y
            y--
        }
        return null
    }

    private fun findAir(
        chunk: Chunk,
        x: Int,
        startY: Int,
        z: Int,
        initialLimit: Int,
    ): Int? {
        var y = startY
        var limit = initialLimit
        while (y <= chunk.verticalBounds.maxY && limit > 0) {
            limit--
            val path = blockPathAt(chunk, x, y, z)
            if (path in COLUMN_CANNOT_PLACE_ON) return null
            if (isAir(chunk, x, y, z)) return y
            y++
        }
        return null
    }

    private fun canPlaceColumnAt(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean {
        if (!isAirOrLavaOcean(chunk, x, y, z)) return false
        val belowState = stateAt(chunk, x, y - 1, z) ?: return false
        return !Blocks.isAir(belowState) && blockPath(belowState) !in COLUMN_CANNOT_PLACE_ON
    }

    private fun isAirOrLavaOcean(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean {
        val state = stateAt(chunk, x, y, z) ?: return false
        return Blocks.isAir(state) || blockPath(state) == "lava" && y <= NETHER_SEA_LEVEL
    }

    private fun isEmpty(stateId: Int): Boolean {
        val path = blockPath(stateId)
        return Blocks.isAir(stateId) || path == "water" || path == "lava"
    }

    private fun isAir(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean = stateAt(chunk, x, y, z)?.let(Blocks::isAir) == true

    private fun isKnownNonAir(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean = stateAt(chunk, x, y, z)?.let { !Blocks.isAir(it) } == true

    private fun stateAt(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ): Int? =
        if (isInChunk(chunk, x, z) && y in chunk.verticalBounds.minY..chunk.verticalBounds.maxY) {
            chunk.getBlockStateId(x and 15, y, z and 15)
        } else {
            null
        }

    private fun blockPathAt(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ): String? = stateAt(chunk, x, y, z)?.let(::blockPath)

    private fun blockPath(stateId: Int): String? = Blocks.byStateId(stateId)?.block?.path

    private fun setBlock(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
    ) {
        if (isInChunk(chunk, x, z) && y in chunk.verticalBounds.minY..chunk.verticalBounds.maxY) {
            chunk.setBlockStateId(x and 15, y, z and 15, stateId)
        }
    }

    private fun isInChunk(
        chunk: Chunk,
        x: Int,
        z: Int,
    ): Boolean = x shr 4 == chunk.chunkX && z shr 4 == chunk.chunkZ

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId
            ?: error("missing vanilla block minecraft:$path")

    private data class Pos(
        val x: Int,
        val y: Int,
        val z: Int,
    )

    private enum class ColumnConfig(
        val minReach: Int,
        val maxReach: Int,
        val minHeight: Int,
        val maxHeight: Int,
    ) {
        SMALL(1, 1, 1, 4),
        LARGE(2, 3, 5, 10),
    }

    private const val SURFACE_STRUCTURES_STEP = 4
    private const val NETHER_SEA_LEVEL = 32
    private val DELTA_CANNOT_REPLACE =
        setOf(
            "bedrock",
            "nether_bricks",
            "nether_brick_fence",
            "nether_brick_stairs",
            "nether_wart",
            "chest",
            "spawner",
        )
    private val COLUMN_CANNOT_PLACE_ON =
        DELTA_CANNOT_REPLACE + setOf("lava", "magma_block", "soul_sand")
}
