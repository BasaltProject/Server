package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.registry.BlockFace
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.withSnowy
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.BiomeClimateData
import org.basaltmc.server.world.terrain.BiomeTemperature
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment

/**
 * // parity: `SnowAndFreezeFeature`, the global Overworld `freeze_top_layer` feature. Vanilla runs
 * it in `TOP_LAYER_MODIFICATION`, after vegetal decoration, and performs one pass over all 256
 * columns using `MOTION_BLOCKING`. Block light is zero during world generation, so its `< 10`
 * check is inherently satisfied here.
 *
 * Snow placement also updates the `snowy` property of grass block, podzol, and mycelium below it.
 */
public object SurfaceFreezingDecoration {
    /** parity: one complete 16x16 `freeze_top_layer` body at the supplied origin. */
    public fun placeAt(
        chunk: Chunk,
        environment: SurfaceRuleEnvironment,
        x: Int,
        z: Int,
    ) {
        for (dx in 0 until 16) {
            for (dz in 0 until 16) {
                val worldX = x + dx
                val worldZ = z + dz
                if (worldX shr 4 != chunk.chunkX || worldZ shr 4 != chunk.chunkZ) continue
                freezeColumn(chunk, environment, worldX and 15, worldZ and 15)
            }
        }
    }

    public fun decorate(
        chunk: Chunk,
        environment: SurfaceRuleEnvironment,
    ): Unit = placeAt(chunk, environment, chunk.chunkX * 16, chunk.chunkZ * 16)

    /** One column: ice on exposed water, snow on top of what can hold it. */
    private fun freezeColumn(
        chunk: Chunk,
        environment: SurfaceRuleEnvironment,
        localX: Int,
        localZ: Int,
    ) {
        val worldX = chunk.chunkX * 16 + localX
        val worldZ = chunk.chunkZ * 16 + localZ
        val topY = chunk.heightAt(localX, localZ, Chunk.HeightmapType.MOTION_BLOCKING)
        if (topY !in WorldHeight.MIN_Y..WorldHeight.MAX_Y) return

        val biomeId = environment.biomeAt(worldX, topY, worldZ)
        val belowY = topY - 1
        if (belowY >= WorldHeight.MIN_Y && shouldFreeze(chunk, localX, belowY, localZ, biomeId, worldX, worldZ)) {
            chunk.setBlockStateId(localX, belowY, localZ, Blocks.ICE.stateId)
        }

        if (shouldSnow(chunk, localX, topY, localZ, biomeId, worldX, worldZ)) {
            chunk.setBlockStateId(localX, topY, localZ, Blocks.SNOW.stateId)
            val snowyBelowStateId = Blocks.withSnowy(chunk.getBlockStateId(localX, belowY, localZ), true)
            if (snowyBelowStateId != null) {
                chunk.setBlockStateId(localX, belowY, localZ, snowyBelowStateId)
            }
        }
    }

    private fun shouldFreeze(
        chunk: Chunk,
        localX: Int,
        y: Int,
        localZ: Int,
        biomeId: String,
        worldX: Int,
        worldZ: Int,
    ): Boolean =
        BiomeTemperature.coldEnoughToSnow(biomeId, worldX, y, worldZ) &&
            blockPath(chunk.getBlockStateId(localX, y, localZ)) == "water"

    private fun shouldSnow(
        chunk: Chunk,
        localX: Int,
        y: Int,
        localZ: Int,
        biomeId: String,
        worldX: Int,
        worldZ: Int,
    ): Boolean {
        if (y <= WorldHeight.MIN_Y) return false
        val climate = BiomeClimateData.forBiomeId(biomeId)
        if (!climate.hasPrecipitation || !BiomeTemperature.coldEnoughToSnow(biomeId, worldX, y, worldZ)) return false
        val stateId = chunk.getBlockStateId(localX, y, localZ)
        if (!Blocks.isAir(stateId) && blockPath(stateId) != "snow") return false
        val belowStateId = chunk.getBlockStateId(localX, y - 1, localZ)
        val belowPath = blockPath(belowStateId)
        return belowPath !in CANNOT_SUPPORT_SNOW_LAYER &&
            (belowPath in SUPPORT_OVERRIDE_SNOW_LAYER || BlockShapes.isCollisionFaceFull(belowStateId, BlockFace.UP))
    }

    private fun blockPath(stateId: Int): String? = Blocks.byStateId(stateId)?.block?.path

    private val CANNOT_SUPPORT_SNOW_LAYER = setOf("ice", "packed_ice", "barrier")
    private val SUPPORT_OVERRIDE_SNOW_LAYER = setOf("honey_block", "soul_sand", "mud")
}
