package org.basaltmc.server.world.noise

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.double
import kotlinx.serialization.json.int

/** Thrown for `worldgen/noise` JSON this parser doesn't understand: an unsupported shape, not malformed JSON. */
public class NoiseParametersParseException(
    message: String,
) : Exception(message)

/**
 * Parses a single real `data/minecraft/worldgen/noise/<name>.json` datapack entry (a registry
 * element, per §7: read from the operator's own downloaded assets at runtime, never embedded in
 * this repo) into [NormalNoise.NoiseParameters]. Matches vanilla's
 * `NormalNoise.NoiseParameters.DIRECT_CODEC`: `{"firstOctave": int, "amplitudes": [double, ...]}`.
 */
public object NoiseParametersJson {
    public fun parse(json: String): NormalNoise.NoiseParameters = parse(Json.parseToJsonElement(json) as JsonObject)

    public fun parse(root: JsonObject): NormalNoise.NoiseParameters {
        val firstOctave =
            (root["firstOctave"] as? JsonPrimitive)?.int
                ?: throw NoiseParametersParseException("Missing 'firstOctave'")
        val amplitudes =
            (root["amplitudes"] as? JsonArray)?.map { (it as JsonPrimitive).double }
                ?: throw NoiseParametersParseException("Missing 'amplitudes'")
        return NormalNoise.NoiseParameters(firstOctave, amplitudes.toDoubleArray())
    }
}
