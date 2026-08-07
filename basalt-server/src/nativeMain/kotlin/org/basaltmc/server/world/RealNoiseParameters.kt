package org.basaltmc.server.world

import org.basaltmc.assets.ServerJarAssets
import org.basaltmc.random.PositionalRandomFactory
import org.basaltmc.world.noise.NoiseParametersJson
import org.basaltmc.world.noise.NormalNoise

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
    private val cache = mutableMapOf<String, NormalNoise>()

    public fun get(id: String): NormalNoise =
        cache.getOrPut(id) {
            val parameters = NoiseParametersJson.parse(serverJar.readText("data/minecraft/worldgen/noise/$id.json"))
            NormalNoise.create(random.fromHashOf("minecraft:$id"), parameters.firstOctave, parameters.amplitudes)
        }
}
