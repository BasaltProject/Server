package org.basaltmc.server.world

import org.basaltmc.assets.ServerJarAssets
import org.basaltmc.random.PositionalRandomFactory
import org.basaltmc.world.noise.NoiseParametersJson
import org.basaltmc.world.noise.NormalNoise
import kotlin.concurrent.Volatile

/**
 * Resolves a named noise (`"continentalness"`, `"offset"`, ...) into a real, correctly-seeded
 * [NormalNoise], the way vanilla's `Noises.instantiate` does: read the real
 * `data/minecraft/worldgen/noise/<id>.json` from the operator's own downloaded [serverJar]
 * (never embedded in this repo, see [ServerJarAssets]'s own KDoc), then seed [NormalNoise] off
 * `random.fromHashOf("minecraft:<id>")`, matching vanilla's own `name.identifier()` (the noise's
 * full `namespace:path` form, always `minecraft:` here since nothing in this project sources
 * noise names from a non-vanilla datapack). Results are cached: every density function that
 * references the same named noise must get the exact same instance, matching vanilla's own
 * registry-level sharing.
 */
public class RealNoiseParameters(
    private val serverJar: ServerJarAssets,
    private val random: PositionalRandomFactory,
) {
    // Copy-on-write, not a plain mutable map: one instance is shared by every generation worker,
    // and a `MutableMap` written from several threads at once corrupts its own backing array, which
    // this project has already been bitten by once (see OverworldBiomeSource's own KDoc). Two
    // threads racing to build the same noise both derive it from the same seed, so either result
    // is equivalent and the loser is simply discarded.
    @Volatile
    private var cache: Map<String, NormalNoise> = emptyMap()

    public fun get(id: String): NormalNoise {
        cache[id]?.let { return it }
        val parameters = NoiseParametersJson.parse(serverJar.readText("data/minecraft/worldgen/noise/$id.json"))
        val noise =
            NormalNoise.create(
                random.fromHashOf("minecraft:$id"),
                parameters.firstOctave,
                parameters.amplitudes,
            )
        return cache[id] ?: noise.also { cache = cache + (id to it) }
    }
}
