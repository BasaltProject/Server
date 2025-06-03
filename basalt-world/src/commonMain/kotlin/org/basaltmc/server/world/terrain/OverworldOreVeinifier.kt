package org.basaltmc.server.world.terrain

import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.BlockState
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.densityfunction.DensityCache
import org.basaltmc.server.world.densityfunction.FunctionContext
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.Mth
import kotlin.math.abs

/** // parity: OreVeinifier, applied after the aquifer material rule and before default stone. */
public class OverworldOreVeinifier(
    private val router: OverworldNoiseRouter,
    seed: Long,
    private val densityCache: DensityCache? = null,
) {
    private data class VeinType(
        val ore: BlockState,
        val rawOreBlock: BlockState,
        val filler: BlockState,
        val minY: Int,
        val maxY: Int,
    )

    private val positionalRandomFactory =
        XoroshiroRandom(seed)
            .forkPositional()
            .fromHashOf("minecraft:ore")
            .forkPositional()

    /** Returns null for default stone or the exact ore/raw-block/filler replacement. */
    public fun compute(
        x: Int,
        y: Int,
        z: Int,
    ): BlockState? {
        val context = FunctionContext(x, y, z, densityCache)
        val veininess = router.veinToggle.compute(context)
        val type = if (veininess > 0.0) COPPER else IRON
        val ridgedVeininess = abs(veininess)
        val distanceFromTop = type.maxY - y
        val distanceFromBottom = y - type.minY
        if (distanceFromBottom < 0 || distanceFromTop < 0) return null

        val distanceFromEdge = minOf(distanceFromTop, distanceFromBottom)
        val edgeRoundoff = Mth.clampedMap(distanceFromEdge.toDouble(), 0.0, 20.0, -0.2, 0.0)
        if (ridgedVeininess + edgeRoundoff < VEININESS_THRESHOLD) return null

        val random = positionalRandomFactory.at(x, y, z)
        if (random.nextFloat() > 0.7f) return null
        if (router.veinRidged.compute(context) >= 0.0) return null

        val richness =
            Mth.clampedMap(
                ridgedVeininess,
                VEININESS_THRESHOLD,
                MAX_RICHNESS_THRESHOLD,
                0.1f.toDouble(),
                0.3f.toDouble(),
            )
        return if (random.nextFloat() < richness && router.veinGap.compute(context) > SKIP_ORE_IF_GAP_NOISE_IS_BELOW) {
            if (random.nextFloat() < 0.02f) type.rawOreBlock else type.ore
        } else {
            type.filler
        }
    }

    private companion object {
        val VEININESS_THRESHOLD = 0.4f.toDouble()
        val MAX_RICHNESS_THRESHOLD = 0.6f.toDouble()
        val SKIP_ORE_IF_GAP_NOISE_IS_BELOW = (-0.3f).toDouble()

        val COPPER =
            VeinType(
                state("copper_ore"),
                state("raw_copper_block"),
                state("granite"),
                0,
                50,
            )
        val IRON =
            VeinType(
                state("deepslate_iron_ore"),
                state("raw_iron_block"),
                state("tuff"),
                -60,
                -8,
            )

        fun state(path: String): BlockState =
            requireNotNull(Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))) {
                "Missing vanilla ore-vein block state minecraft:$path"
            }
    }
}
