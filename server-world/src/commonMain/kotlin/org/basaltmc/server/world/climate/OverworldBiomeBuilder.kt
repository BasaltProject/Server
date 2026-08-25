package org.basaltmc.server.world.climate

import org.basaltmc.server.registry.Biome

/**
 * // parity: net.minecraft.world.level.biome.OverworldBiomeBuilder, ported mechanically: every
 * threshold, table and branch below is vanilla's, not an approximation.
 *
 * Biomes are regions in the six-axis climate space, and [ParameterList.findValue] picks the
 * closest one, breaking ties between equally good points by first-registered-wins. The call
 * order below, and inside every function it calls, is therefore semantically significant and
 * must never be reordered. The build-up is split across sibling files in this package:
 * [OverworldClimateTables] holds the static tables, `OverworldBiomeEmit.kt` the emit primitives.
 */
public class OverworldBiomeBuilder {
    private val tables = OverworldClimateTables()

    /** // parity: OverworldBiomeBuilder.addBiomes. [SharedConstants.debugGenerateSquareTerrainWithoutNoise] is never true in this project: `addDebugBiomes` is not ported. */
    public fun addBiomes(emit: (ParameterPoint, Biome) -> Unit) {
        tables.addOffCoastBiomes(emit)
        addInlandBiomes(emit)
        tables.addUndergroundBiomes(emit)
    }

    public fun build(): ParameterList<Biome> {
        val values = mutableListOf<Pair<ParameterPoint, Biome>>()
        addBiomes { point, biome -> values += point to biome }
        return ParameterList(values)
    }

    private fun addInlandBiomes(emit: (ParameterPoint, Biome) -> Unit) {
        tables.addMidSlice(emit, Parameter.span(-1.0f, -0.93333334f))
        tables.addHighSlice(emit, Parameter.span(-0.93333334f, -0.7666667f))
        tables.addPeaks(emit, Parameter.span(-0.7666667f, -0.56666666f))
        tables.addHighSlice(emit, Parameter.span(-0.56666666f, -0.4f))
        tables.addMidSlice(emit, Parameter.span(-0.4f, -0.26666668f))
        tables.addLowSlice(emit, Parameter.span(-0.26666668f, -0.05f))
        tables.addValleys(emit, Parameter.span(-0.05f, 0.05f))
        tables.addLowSlice(emit, Parameter.span(0.05f, 0.26666668f))
        tables.addMidSlice(emit, Parameter.span(0.26666668f, 0.4f))
        tables.addHighSlice(emit, Parameter.span(0.4f, 0.56666666f))
        tables.addPeaks(emit, Parameter.span(0.56666666f, 0.7666667f))
        tables.addHighSlice(emit, Parameter.span(0.7666667f, 0.93333334f))
        tables.addMidSlice(emit, Parameter.span(0.93333334f, 1.0f))
    }
}
