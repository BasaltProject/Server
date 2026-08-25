package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.hasWaterFluid
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment

internal fun grassDiskStateForAbove(aboveStateId: Int?): Int =
    if (aboveStateId != null &&
        (BlockShapes.isSolidState(aboveStateId) || Blocks.hasWaterFluid(aboveStateId))
    ) {
        Blocks.DIRT.stateId
    } else {
        Blocks.GRASS_BLOCK.stateId
    }

internal fun placeDiskGrass(
    region: PlacementRegion,
    random: WorldgenRandom,
    environment: SurfaceRuleEnvironment,
) {
    val x = region.chunk.chunkX * 16 + random.nextInt(16)
    val z = region.chunk.chunkZ * 16 + random.nextInt(16)
    val y = (region.oceanFloorHeight(x, z) ?: return) - 1
    if (region.getBlockStateId(x, y, z) != mudStateId) return
    if (environment.biomeAt(x, y, z).substringAfter(':') != "mangrove_swamp") return
    placeDisk(region, random, x, y, z, GRASS_DISK)
}

internal fun placeWaterDisk(
    region: PlacementRegion,
    random: WorldgenRandom,
    environment: SurfaceRuleEnvironment,
    config: DiskConfig,
    attempts: Int,
    biomes: Set<String>,
) {
    repeat(attempts) {
        val x = region.chunk.chunkX * 16 + random.nextInt(16)
        val z = region.chunk.chunkZ * 16 + random.nextInt(16)
        val y = region.oceanFloorHeight(x, z) ?: return@repeat
        if (region.getBlockStateId(x, y, z)?.let(Blocks::hasWaterFluid) != true) return@repeat
        if (environment.biomeAt(x, y, z).substringAfter(':') !in biomes) return@repeat
        placeDisk(region, random, x, y, z, config)
    }
}

internal fun placeDisk(
    region: PlacementRegion,
    random: WorldgenRandom,
    originX: Int,
    originY: Int,
    originZ: Int,
    config: DiskConfig,
) {
    val radius = config.minRadius + random.nextInt(config.maxRadius - config.minRadius + 1)
    val radiusSquared = radius * radius
    for (z in originZ - radius..originZ + radius) {
        for (x in originX - radius..originX + radius) {
            val dx = x - originX
            val dz = z - originZ
            if (dx * dx + dz * dz > radiusSquared) continue
            for (y in originY + config.halfHeight downTo originY - config.halfHeight) {
                val current = region.getBlockStateId(x, y, z) ?: continue
                if (current !in config.targets) continue
                region.setBlockStateId(x, y, z, config.state(region, x, y, z))
            }
        }
    }
}

internal data class DiskConfig(
    val minRadius: Int,
    val maxRadius: Int,
    val halfHeight: Int,
    val targets: Set<Int>,
    val state: (PlacementRegion, Int, Int, Int) -> Int,
)

private val sandStateId: Int = stateId("sand")
private val sandstoneStateId: Int = stateId("sandstone")
private val clayStateId: Int = stateId("clay")
private val gravelStateId: Int = stateId("gravel")
private val mudStateId: Int = stateId("mud")

internal val SAND_DISK: DiskConfig =
    DiskConfig(2, 6, 2, setOf(Blocks.DIRT.stateId, Blocks.GRASS_BLOCK.stateId)) { region, x, y, z ->
        if (region.getBlockStateId(x, y - 1, z) == Blocks.AIR.stateId) sandstoneStateId else sandStateId
    }
internal val CLAY_DISK: DiskConfig =
    DiskConfig(2, 3, 1, setOf(Blocks.DIRT.stateId, clayStateId)) { _, _, _, _ -> clayStateId }
internal val GRAVEL_DISK: DiskConfig =
    DiskConfig(2, 5, 2, setOf(Blocks.DIRT.stateId, Blocks.GRASS_BLOCK.stateId)) { _, _, _, _ -> gravelStateId }
internal val GRASS_DISK: DiskConfig =
    DiskConfig(2, 6, 2, setOf(Blocks.DIRT.stateId, mudStateId)) { region, x, y, z ->
        grassDiskStateForAbove(region.getBlockStateId(x, y + 1, z))
    }
