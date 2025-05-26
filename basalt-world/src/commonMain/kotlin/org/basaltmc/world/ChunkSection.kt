package org.basaltmc.world

import org.basaltmc.registry.Blocks

/** One 16x16x16 slice of a [Chunk]. Block state IDs default to 0, which is [Blocks.AIR]'s ID. */
public class ChunkSection {
    private val blockStateIds = IntArray(16 * 16 * 16)

    /** Biome network IDs at quart (4-block) resolution, 4x4x4 per section; default 0 (plains). */
    private val biomeIds = IntArray(64)

    public fun getBlockStateId(
        x: Int,
        y: Int,
        z: Int,
    ): Int = blockStateIds[index(x, y, z)]

    public fun setBlockStateId(
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
    ) {
        val at = index(x, y, z)
        val previous = blockStateIds[at]
        if (previous == stateId) return
        blockStateIds[at] = stateId
        if (previous != Blocks.AIR.stateId) trackedNonAirBlockCount--
        if (stateId != Blocks.AIR.stateId) trackedNonAirBlockCount++
        if (Blocks.hasWaterFluid(previous)) trackedNonEmptyFluidCount--
        if (Blocks.hasWaterFluid(stateId)) trackedNonEmptyFluidCount++
    }

    public fun getBiomeId(
        quartX: Int,
        quartY: Int,
        quartZ: Int,
    ): Int = biomeIds[biomeIndex(quartX, quartY, quartZ)]

    public fun setBiomeId(
        quartX: Int,
        quartY: Int,
        quartZ: Int,
        biomeId: Int,
    ) {
        biomeIds[biomeIndex(quartX, quartY, quartZ)] = biomeId
    }

    /** A copy of the raw biome IDs, in `(y * 4 + z) * 4 + x` order: the layout the biome `PalettedContainer` expects. */
    public fun biomeIds(): IntArray = biomeIds.copyOf()

    // Both counts are maintained on write rather than recounted on read: they used to scan all 4096
    // entries every time they were read, which is once per section per chunk send.
    private var trackedNonAirBlockCount = 0
    private var trackedNonEmptyFluidCount = 0

    /** Blocks that count toward lighting (i.e. not air), matching vanilla's per-section block count. */
    public val nonAirBlockCount: Int
        get() = trackedNonAirBlockCount

    /** Blocks whose default state carries water, matching vanilla's section fluid-state count. */
    public val nonEmptyFluidCount: Int
        get() = trackedNonEmptyFluidCount

    /** A copy of the raw state IDs, in `(y * 16 + z) * 16 + x` order: the layout `PalettedContainer` expects. */
    public fun blockStateIds(): IntArray = blockStateIds.copyOf()

    /** Overwrites every state ID at once, in the same `(y * 16 + z) * 16 + x` order [blockStateIds] returns. */
    public fun setBlockStateIds(ids: IntArray) {
        require(ids.size == blockStateIds.size) { "expected ${blockStateIds.size} state ids, got ${ids.size}" }
        ids.copyInto(blockStateIds)
        trackedNonAirBlockCount = blockStateIds.count { it != Blocks.AIR.stateId }
        trackedNonEmptyFluidCount = blockStateIds.count(Blocks::hasWaterFluid)
    }

    private fun index(
        x: Int,
        y: Int,
        z: Int,
    ): Int {
        require(x in 0..15 && y in 0..15 && z in 0..15) { "coordinates out of section bounds: ($x, $y, $z)" }
        return (y shl 8) or (z shl 4) or x
    }

    private fun biomeIndex(
        quartX: Int,
        quartY: Int,
        quartZ: Int,
    ): Int {
        require(
            quartX in 0..3 && quartY in 0..3 && quartZ in 0..3,
        ) {
            "quart coordinates out of section bounds: ($quartX, $quartY, $quartZ)"
        }
        return (quartY shl 4) or (quartZ shl 2) or quartX
    }

}
