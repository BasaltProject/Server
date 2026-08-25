package org.basaltmc.server.world.terrain.decoration.nether

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.growingVineHeadStateId
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.terrain.NetherBiomeSource

internal fun NetherVegetalDecoration.placeWeepingVines(
    chunk: Chunk,
    random: BasaltRandom,
    biomeSource: NetherBiomeSource,
) {
    repeat(10) {
        val x = chunk.chunkX * 16 + random.nextInt(16)
        val z = chunk.chunkZ * 16 + random.nextInt(16)
        val y = random.nextInt(128)
        if (biomeSource.biomeAt(x, y, z).id.path == "crimson_forest") {
            placeWeepingVinesFeature(chunk, random, Pos(x, y, z))
        }
    }
}

internal fun NetherVegetalDecoration.placeWeepingVinesFeature(
    chunk: Chunk,
    random: BasaltRandom,
    origin: Pos,
) {
    if (!isAir(chunk, origin.x, origin.y, origin.z)) return
    if (blockPathAt(chunk, origin.x, origin.y + 1, origin.z) !in WEEPING_ROOF_SUPPORT) return
    setBlock(chunk, origin.x, origin.y, origin.z, stateId("nether_wart_block"))
    repeat(200) {
        val x = origin.x + random.nextInt(6) - random.nextInt(6)
        val y = origin.y + random.nextInt(2) - random.nextInt(5)
        val z = origin.z + random.nextInt(6) - random.nextInt(6)
        if (!isAir(chunk, x, y, z)) return@repeat
        val neighbours =
            ALL_NEIGHBOURS.count { (offsetX, offsetY, offsetZ) ->
                blockPathAt(chunk, x + offsetX, y + offsetY, z + offsetZ) in WEEPING_ROOF_SUPPORT
            }
        if (neighbours == 1) setBlock(chunk, x, y, z, stateId("nether_wart_block"))
    }
    repeat(100) {
        val x = origin.x + random.nextInt(8) - random.nextInt(8)
        val y = origin.y + random.nextInt(2) - random.nextInt(7)
        val z = origin.z + random.nextInt(8) - random.nextInt(8)
        if (!isAir(chunk, x, y, z) || blockPathAt(chunk, x, y + 1, z) !in WEEPING_ROOF_SUPPORT) return@repeat
        var vineHeight = 1 + random.nextInt(8)
        if (random.nextInt(6) == 0) vineHeight *= 2
        if (random.nextInt(5) == 0) vineHeight = 1
        placeWeepingColumn(chunk, random, x, y, z, vineHeight, 17)
    }
}

internal fun NetherVegetalDecoration.placeWeepingColumn(
    chunk: Chunk,
    random: BasaltRandom,
    x: Int,
    startY: Int,
    z: Int,
    totalHeight: Int,
    minAge: Int,
) {
    var y = startY
    for (height in 0..totalHeight) {
        if (isAir(chunk, x, y, z)) {
            if (height == totalHeight || !isAir(chunk, x, y - 1, z)) {
                val age = minAge + random.nextInt(26 - minAge)
                val default = stateId("weeping_vines")
                val head = Blocks.growingVineHeadStateId(default, age) ?: error("missing weeping vines age $age")
                setBlock(chunk, x, y, z, head)
                break
            }
            setBlock(chunk, x, y, z, stateId("weeping_vines_plant"))
        }
        y--
    }
}

internal fun NetherVegetalDecoration.vineHeadState(
    path: String,
    random: BasaltRandom,
): Int {
    val age = 17 + random.nextInt(9)
    return Blocks.growingVineHeadStateId(stateId(path), age) ?: error("missing $path age $age")
}

private val WEEPING_ROOF_SUPPORT = setOf("netherrack", "nether_wart_block")
private val ALL_NEIGHBOURS =
    listOf(
        Triple(0, -1, 0),
        Triple(0, 1, 0),
        Triple(0, 0, -1),
        Triple(0, 0, 1),
        Triple(-1, 0, 0),
        Triple(1, 0, 0),
    )
