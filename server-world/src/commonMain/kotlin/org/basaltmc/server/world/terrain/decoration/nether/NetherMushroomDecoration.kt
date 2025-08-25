package org.basaltmc.server.world.terrain.decoration.nether

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.terrain.NetherBiomeLookup

internal fun NetherVegetalDecoration.placeNormalMushroom(
    chunk: Chunk,
    random: BasaltRandom,
    biomeSource: NetherBiomeLookup,
    rarity: Int,
    mushroom: String,
) {
    if (random.nextFloat() >= 1.0f / rarity) return
    val baseX = chunk.chunkX * 16 + random.nextInt(16)
    val baseZ = chunk.chunkZ * 16 + random.nextInt(16)
    val baseY = chunk.heightAt(baseX and 15, baseZ and 15, Chunk.HeightmapType.MOTION_BLOCKING)
    if (biomeSource.blockBiomeAt(baseX, baseY, baseZ).id.path !in NORMAL_MUSHROOM_BIOMES) return
    val mushroomState = stateId(mushroom)
    repeat(PATCH_ATTEMPTS) {
        val x = baseX + triangle(random, 7)
        val y = baseY + triangle(random, 3)
        val z = baseZ + triangle(random, 7)
        val support = blockPathAt(chunk, x, y - 1, z)
        if (isAir(chunk, x, y, z) && support != null && supportsMushroom(support)) {
            setBlock(chunk, x, y, z, mushroomState)
        }
    }
}

internal fun NetherVegetalDecoration.supportsMushroom(path: String): Boolean =
    path in MUSHROOM_LIGHT_OVERRIDE || path in MUSHROOM_SOLID_SUPPORT

private const val PATCH_ATTEMPTS = 96
private val NORMAL_MUSHROOM_BIOMES = setOf("nether_wastes", "crimson_forest", "warped_forest")
private val MUSHROOM_LIGHT_OVERRIDE = setOf("mycelium", "podzol", "crimson_nylium", "warped_nylium")
private val MUSHROOM_SOLID_SUPPORT =
    setOf(
        "netherrack",
        "basalt",
        "blackstone",
        "bedrock",
        "soul_sand",
        "soul_soil",
        "gravel",
        "magma_block",
        "nether_gold_ore",
        "nether_quartz_ore",
        "ancient_debris",
    )
