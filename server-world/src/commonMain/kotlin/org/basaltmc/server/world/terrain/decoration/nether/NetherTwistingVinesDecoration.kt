package org.basaltmc.server.world.terrain.decoration.nether

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.terrain.NetherBiomeLookup

internal fun NetherVegetalDecoration.placeTwistingVines(
    chunk: Chunk,
    random: BasaltRandom,
    biomeSource: NetherBiomeLookup,
) {
    repeat(10) {
        val x = chunk.chunkX * 16 + random.nextInt(16)
        val z = chunk.chunkZ * 16 + random.nextInt(16)
        val y = random.nextInt(128)
        if (biomeSource.blockBiomeAt(x, y, z).id.path == "warped_forest") {
            placeTwistingVinesFeature(chunk, random, Pos(x, y, z))
        }
    }
}

internal fun NetherVegetalDecoration.placeTwistingVinesFeature(
    chunk: Chunk,
    random: BasaltRandom,
    origin: Pos,
) {
    if (isInvalidTwistingPosition(chunk, origin.x, origin.y, origin.z)) return
    repeat(64) {
        val x = origin.x - 8 + random.nextInt(17)
        var y = origin.y - 4 + random.nextInt(9)
        val z = origin.z - 8 + random.nextInt(17)
        while (true) {
            y--
            if (y !in chunk.verticalBounds.minY..chunk.verticalBounds.maxY) return@repeat
            if (!isAir(chunk, x, y, z)) break
        }
        y++
        if (isInvalidTwistingPosition(chunk, x, y, z)) return@repeat
        var vineHeight = 1 + random.nextInt(8)
        if (random.nextInt(6) == 0) vineHeight *= 2
        if (random.nextInt(5) == 0) vineHeight = 1
        placeTwistingColumn(chunk, random, x, y, z, vineHeight)
    }
}

internal fun NetherVegetalDecoration.isInvalidTwistingPosition(
    chunk: Chunk,
    x: Int,
    y: Int,
    z: Int,
): Boolean = !isAir(chunk, x, y, z) || blockPathAt(chunk, x, y - 1, z) !in TWISTING_SUPPORT

internal fun NetherVegetalDecoration.placeTwistingColumn(
    chunk: Chunk,
    random: BasaltRandom,
    x: Int,
    startY: Int,
    z: Int,
    totalHeight: Int,
) {
    var y = startY
    for (height in 1..totalHeight) {
        if (isAir(chunk, x, y, z)) {
            if (height == totalHeight || !isAir(chunk, x, y + 1, z)) {
                setBlock(chunk, x, y, z, vineHeadState("twisting_vines", random))
                break
            }
            setBlock(chunk, x, y, z, stateId("twisting_vines_plant"))
        }
        y++
    }
}

private val TWISTING_SUPPORT = setOf("netherrack", "warped_nylium", "warped_wart_block")
