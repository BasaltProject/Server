package org.basaltmc.world.terrain.decoration

import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk
import org.basaltmc.world.WorldHeight
import org.basaltmc.world.terrain.BiomeClimateData
import org.basaltmc.world.terrain.BiomeTemperature
import org.basaltmc.world.terrain.surface.SurfaceRuleEnvironment

/**
 * // parity: `SnowAndFreezeFeature`, the global Overworld `freeze_top_layer` feature. Vanilla runs
 * it in `TOP_LAYER_MODIFICATION`, after vegetal decoration, and performs one pass over all 256
 * columns using `MOTION_BLOCKING`. Block light is zero during world generation, so its `< 10`
 * check is inherently satisfied here.
 *
 * The final `SnowyBlock.SNOWY=true` mutation is intentionally absent because this project still
 * represents only default block states and cannot encode block properties.
 */
public object SurfaceFreezingDecoration {
    public fun decorate(
        chunk: Chunk,
        environment: SurfaceRuleEnvironment,
    ) {
        for (localX in 0 until 16) {
            for (localZ in 0 until 16) {
                val worldX = chunk.chunkX * 16 + localX
                val worldZ = chunk.chunkZ * 16 + localZ
                val topY = chunk.heightAt(localX, localZ, Chunk.HeightmapType.MOTION_BLOCKING)
                if (topY !in WorldHeight.MIN_Y..WorldHeight.MAX_Y) continue

                val biomeId = environment.biomeAt(worldX, topY, worldZ)
                val belowY = topY - 1
                if (belowY >= WorldHeight.MIN_Y && shouldFreeze(chunk, localX, belowY, localZ, biomeId, worldX, worldZ)) {
                    chunk.setBlockStateId(localX, belowY, localZ, Blocks.ICE.stateId)
                }

                if (shouldSnow(chunk, localX, topY, localZ, biomeId, worldX, worldZ)) {
                    chunk.setBlockStateId(localX, topY, localZ, Blocks.SNOW.stateId)
                }
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
            chunk.getBlockStateId(localX, y, localZ) == Blocks.WATER.stateId

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
        if (stateId != Blocks.AIR.stateId && stateId != Blocks.SNOW.stateId) return false
        val belowStateId = chunk.getBlockStateId(localX, y - 1, localZ)
        return belowStateId !in cannotSupportSnowStateIds &&
            Chunk.HeightmapType.OCEAN_FLOOR.matches(belowStateId)
    }

    private val cannotSupportSnowStateIds: Set<Int> =
        listOf("ice", "packed_ice", "barrier")
            .mapNotNull { Blocks.REGISTRY.byIdentifier(Identifier.vanilla(it))?.stateId }
            .toSet()
}
