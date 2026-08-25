package org.basaltmc.server.world.terrain.decoration.nether

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.terrain.NetherBiomeSource

internal fun NetherVegetalDecoration.placeLavaSprings(
    chunk: Chunk,
    random: BasaltRandom,
    biomeSource: NetherBiomeSource,
) {
    repeat(20) {
        val x = chunk.chunkX * 16 + random.nextInt(16)
        val z = chunk.chunkZ * 16 + random.nextInt(16)
        val y = veryBiasedHeight(random)
        if (biomeSource.biomeAt(x, y, z).id.path in ALL_BIOMES) {
            placeLavaSpring(chunk, x, y, z)
        }
    }
}

internal fun NetherVegetalDecoration.veryBiasedHeight(random: BasaltRandom): Int {
    val upperInclusive = 8 + random.nextInt(112)
    val biasedUpperInclusive = random.nextInt(upperInclusive)
    return random.nextInt(biasedUpperInclusive + 8)
}

internal fun NetherVegetalDecoration.placeLavaSpring(
    chunk: Chunk,
    x: Int,
    y: Int,
    z: Int,
) {
    if (!isSpringRock(chunk, x, y + 1, z) || !isSpringRock(chunk, x, y - 1, z)) return
    val current = terrainStateAt(chunk, x, y, z) ?: return
    if (!Blocks.isAir(current) && !isSpringRock(chunk, x, y, z)) return
    var rocks = 0
    var holes = 0
    for ((offsetX, offsetY, offsetZ) in SPRING_NEIGHBOURS) {
        val state = terrainStateAt(chunk, x + offsetX, y + offsetY, z + offsetZ)
        if (state != null && blockPath(state) in OVERWORLD_SPRING_ROCKS) rocks++
        if (state != null && Blocks.isAir(state)) holes++
    }
    if (rocks == 4 && holes == 1) setBlock(chunk, x, y, z, Blocks.LAVA.stateId)
}

internal fun NetherVegetalDecoration.isSpringRock(
    chunk: Chunk,
    x: Int,
    y: Int,
    z: Int,
): Boolean = terrainStateAt(chunk, x, y, z)?.let { blockPath(it) } in OVERWORLD_SPRING_ROCKS

private val ALL_BIOMES =
    setOf("nether_wastes", "soul_sand_valley", "crimson_forest", "warped_forest", "basalt_deltas")
private val OVERWORLD_SPRING_ROCKS =
    setOf("stone", "granite", "diorite", "andesite", "deepslate", "tuff", "calcite", "dirt")
private val SPRING_NEIGHBOURS =
    listOf(
        Triple(-1, 0, 0),
        Triple(1, 0, 0),
        Triple(0, 0, -1),
        Triple(0, 0, 1),
        Triple(0, -1, 0),
    )
