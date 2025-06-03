package org.basaltmc.world.terrain.decoration.nether

import org.basaltmc.common.Identifier
import org.basaltmc.random.BasaltRandom
import org.basaltmc.random.WorldgenRandom
import org.basaltmc.random.XoroshiroRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.world.Chunk
import org.basaltmc.world.terrain.NetherBiomeSource
import kotlin.math.abs

/** // parity: Nether basalt_pillar in the LOCAL_MODIFICATIONS step. */
public object NetherLocalDecoration {
    public fun decorate(
        chunk: Chunk,
        seed: Long,
        biomeSource: NetherBiomeSource,
    ) {
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        random.setFeatureSeed(decorationSeed, BASALT_PILLAR_INDEX, LOCAL_MODIFICATIONS_STEP)
        repeat(10) {
            val x = chunk.chunkX * 16 + random.nextInt(16)
            val z = chunk.chunkZ * 16 + random.nextInt(16)
            val y = random.nextInt(128)
            if (biomeSource.biomeAt(x, y, z).id.path == "soul_sand_valley") {
                placePillar(chunk, random, x, y, z)
            }
        }
    }

    private fun placePillar(
        chunk: Chunk,
        random: BasaltRandom,
        originX: Int,
        originY: Int,
        originZ: Int,
    ) {
        if (!isAir(chunk, originX, originY, originZ) || isAir(chunk, originX, originY + 1, originZ)) return
        val basalt = stateId("basalt")
        var y = originY
        var placeNorth = true
        var placeSouth = true
        var placeWest = true
        var placeEast = true

        while (true) {
            if (y !in chunk.verticalBounds.minY..chunk.verticalBounds.maxY) return
            if (!isAir(chunk, originX, y, originZ)) break
            setBlock(chunk, originX, y, originZ, basalt)
            placeNorth = placeNorth && placeHangOff(chunk, random, originX, y, originZ - 1, basalt)
            placeSouth = placeSouth && placeHangOff(chunk, random, originX, y, originZ + 1, basalt)
            placeWest = placeWest && placeHangOff(chunk, random, originX - 1, y, originZ, basalt)
            placeEast = placeEast && placeHangOff(chunk, random, originX + 1, y, originZ, basalt)
            y--
        }

        val lastPillarY = y + 1
        placeBaseHangOff(chunk, random, originX, lastPillarY, originZ - 1, basalt)
        placeBaseHangOff(chunk, random, originX, lastPillarY, originZ + 1, basalt)
        placeBaseHangOff(chunk, random, originX - 1, lastPillarY, originZ, basalt)
        placeBaseHangOff(chunk, random, originX + 1, lastPillarY, originZ, basalt)
        placeBase(chunk, random, originX, y, originZ, basalt)
    }

    private fun placeBase(
        chunk: Chunk,
        random: BasaltRandom,
        originX: Int,
        originY: Int,
        originZ: Int,
        basalt: Int,
    ) {
        for (offsetX in -3..3) {
            for (offsetZ in -3..3) {
                val probability = abs(offsetX) * abs(offsetZ)
                if (random.nextInt(10) >= 10 - probability) continue
                val x = originX + offsetX
                val z = originZ + offsetZ
                var y = originY
                var maxDrop = 3
                while (isAir(chunk, x, y - 1, z)) {
                    y--
                    if (--maxDrop <= 0) break
                }
                if (!isAir(chunk, x, y - 1, z)) setBlock(chunk, x, y, z, basalt)
            }
        }
    }

    private fun placeHangOff(
        chunk: Chunk,
        random: BasaltRandom,
        x: Int,
        y: Int,
        z: Int,
        basalt: Int,
    ): Boolean {
        if (random.nextInt(10) == 0) return false
        setBlock(chunk, x, y, z, basalt)
        return true
    }

    private fun placeBaseHangOff(
        chunk: Chunk,
        random: BasaltRandom,
        x: Int,
        y: Int,
        z: Int,
        basalt: Int,
    ) {
        if (random.nextBoolean()) setBlock(chunk, x, y, z, basalt)
    }

    private fun isAir(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean =
        if (isInChunk(chunk, x, z) && y in chunk.verticalBounds.minY..chunk.verticalBounds.maxY) {
            Blocks.isAir(chunk.getBlockStateId(x and 15, y, z and 15))
        } else {
            false
        }

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

    private const val LOCAL_MODIFICATIONS_STEP = 2
    private const val BASALT_PILLAR_INDEX = 0
}
