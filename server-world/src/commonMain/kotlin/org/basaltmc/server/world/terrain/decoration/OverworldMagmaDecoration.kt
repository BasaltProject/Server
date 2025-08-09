package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.BlockFace
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment

internal fun placeUnderwaterMagma(
    region: PlacementRegion,
    random: WorldgenRandom,
    environment: SurfaceRuleEnvironment,
) {
    val attempts = 44 + random.nextInt(9)
    repeat(attempts) {
        val x = region.chunk.chunkX * 16 + random.nextInt(16)
        val z = region.chunk.chunkZ * 16 + random.nextInt(16)
        val y = WorldHeight.MIN_Y + random.nextInt(321)
        val oceanFloor = region.oceanFloorHeight(x, z) ?: return@repeat
        if (y > oceanFloor - 2 || environment.biomeAt(x, y, z).substringAfter(':') !in ALL_BIOMES) return@repeat
        placeUnderwaterMagmaBody(region, random, x, y, z)
    }
}

internal fun placeUnderwaterMagmaBody(
    region: PlacementRegion,
    random: WorldgenRandom,
    x: Int,
    y: Int,
    z: Int,
): Boolean {
    val floorY = findWaterColumnFloor(region, x, y, z) ?: return false
    var placed = false
    for (placedZ in z - 1..z + 1) {
        for (placedY in floorY - 1..floorY + 1) {
            for (placedX in x - 1..x + 1) {
                if (random.nextFloat() >= 0.5f || !isValidMagmaPlacement(region, placedX, placedY, placedZ)) {
                    continue
                }
                region.setBlockStateId(placedX, placedY, placedZ, magmaStateId)
                placed = true
            }
        }
    }
    return placed
}

internal fun findWaterColumnFloor(
    region: PlacementRegion,
    x: Int,
    y: Int,
    z: Int,
): Int? {
    if (!isUnderwaterMagmaWater(region.getBlockStateId(x, y, z))) return null
    var scanY = y
    var distance = 1
    while (distance < 5 && isUnderwaterMagmaWater(region.getBlockStateId(x, scanY, z))) {
        scanY--
        distance++
    }
    return if (!isUnderwaterMagmaWater(region.getBlockStateId(x, scanY, z))) scanY else null
}

internal fun isUnderwaterMagmaWater(stateId: Int?): Boolean =
    stateId != null && Blocks.byStateId(stateId)?.block?.path == "water"

internal fun isValidMagmaPlacement(
    region: PlacementRegion,
    x: Int,
    y: Int,
    z: Int,
): Boolean {
    val current = region.getBlockStateId(x, y, z)
    if (current == null || Blocks.isAir(current) || Blocks.byStateId(current)?.block?.path == "water") return false
    if (!hasFullMagmaCover(region.getBlockStateId(x, y - 1, z), BlockFace.UP)) return false
    return hasFullMagmaCover(region.getBlockStateId(x + 1, y, z), BlockFace.WEST) &&
        hasFullMagmaCover(region.getBlockStateId(x - 1, y, z), BlockFace.EAST) &&
        hasFullMagmaCover(region.getBlockStateId(x, y, z + 1), BlockFace.NORTH) &&
        hasFullMagmaCover(region.getBlockStateId(x, y, z - 1), BlockFace.SOUTH)
}

internal fun hasFullMagmaCover(
    stateId: Int?,
    coveredFace: BlockFace,
): Boolean = stateId != null && BlockShapes.isFaceOcclusionFull(stateId, coveredFace)

private val magmaStateId: Int = stateId("magma_block")
