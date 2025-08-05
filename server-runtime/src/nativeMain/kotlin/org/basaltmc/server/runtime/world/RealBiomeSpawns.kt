package org.basaltmc.server.runtime.world

import org.basaltmc.server.assets.ServerJarAssets
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.logging.Logger
import kotlin.concurrent.Volatile

/** One entry of a biome's spawn list: what can appear there, how likely, and how many at a time. */
public class SpawnerEntry(
    public val type: Identifier,
    public val weight: Int,
    public val minCount: Int,
    public val maxCount: Int,
)

/** How strongly one mob repels another of the same biome-defined cost family. */
public class MobSpawnCost(
    public val energyBudget: Double,
    public val charge: Double,
)

private class BiomeSpawnSettings(
    val spawners: Map<String, List<SpawnerEntry>>,
    val creatureProbability: Float,
    val costs: Map<Identifier, MobSpawnCost>,
)

/**
 * What each biome can spawn, out of the operator's own `server.jar`.
 *
 * parity: `MobSpawnSettings`, the `spawners` block of every
 * `data/minecraft/worldgen/biome/<name>.json`. Datapack content, so it is read from the download
 * rather than committed, the same as loot tables and recipes. Which of those entries actually
 * appears is [NaturalSpawner]'s business; this only says what the biome offers.
 */
public class RealBiomeSpawns(
    private val serverJar: ServerJarAssets?,
    private val logger: Logger = Logger("BiomeSpawns"),
) {
    @Volatile
    private var cache: Map<String, BiomeSpawnSettings> = emptyMap()

    /** The spawn list of [biome] for [category], empty when the biome offers nothing of that kind. */
    public fun listFor(
        biome: Identifier,
        category: String,
    ): List<SpawnerEntry> = of(biome)[category].orEmpty()

    /** parity: `MobSpawnSettings.creatureGenerationProbability`, defaulting to 0.1. */
    public fun creatureProbabilityFor(biome: Identifier): Float = settingsOf(biome).creatureProbability

    /** parity: `MobSpawnSettings.getMobSpawnCost`, absent for almost every biome and entity. */
    public fun costFor(
        biome: Identifier,
        type: Identifier,
    ): MobSpawnCost? = settingsOf(biome).costs[type]

    public fun of(biome: Identifier): Map<String, List<SpawnerEntry>> = settingsOf(biome).spawners

    private fun settingsOf(biome: Identifier): BiomeSpawnSettings {
        val key = biome.path
        cache[key]?.let { return it }
        val jar = serverJar ?: return EMPTY_SETTINGS
        val parsed =
            runCatching {
                val json = jar.readText("data/minecraft/worldgen/biome/$key.json")
                BiomeSpawnSettings(
                    spawners = parse(json),
                    creatureProbability = creatureProbability(json),
                    costs = parseCosts(json),
                )
            }.onFailure { logger.debug { "No spawn list for biome '$key': ${it.message}" } }
                .getOrDefault(EMPTY_SETTINGS)
        cache = cache + (key to parsed)
        return parsed
    }

    /**
     * Reads the `spawners` object without a full JSON model: it is a map of category to a list of
     * flat objects, and everything else in a biome file is worldgen's business rather than this one's.
     */
    private fun parse(json: String): Map<String, List<SpawnerEntry>> {
        val body = objectBody(json, "spawners") ?: return emptyMap()
        val lists = mutableMapOf<String, List<SpawnerEntry>>()
        var cursor = 0
        while (true) {
            val nameStart = body.indexOf('"', cursor)
            if (nameStart < 0) break
            val nameEnd = body.indexOf('"', nameStart + 1)
            if (nameEnd < 0) break
            val category = body.substring(nameStart + 1, nameEnd)
            val arrayStart = body.indexOf('[', nameEnd)
            if (arrayStart < 0) break
            val arrayEnd = body.indexOf(']', arrayStart)
            if (arrayEnd < 0) break
            lists[category] = entriesOf(body.substring(arrayStart + 1, arrayEnd))
            cursor = arrayEnd + 1
        }
        return lists
    }

    private fun parseCosts(json: String): Map<Identifier, MobSpawnCost> {
        val body = objectBody(json, "spawn_costs") ?: return emptyMap()
        val costs = mutableMapOf<Identifier, MobSpawnCost>()
        var cursor = 0
        while (true) {
            val nameStart = body.indexOf('"', cursor)
            if (nameStart < 0) break
            val nameEnd = body.indexOf('"', nameStart + 1)
            if (nameEnd < 0) break
            val type = runCatching { Identifier.parse(body.substring(nameStart + 1, nameEnd)) }.getOrNull()
            val nestedOpen = body.indexOf('{', nameEnd)
            if (nestedOpen < 0) break
            val nestedClose = matchingBrace(body, nestedOpen) ?: break
            val fields = body.substring(nestedOpen + 1, nestedClose)
            val energyBudget = doubleField(fields, "energy_budget")
            val charge = doubleField(fields, "charge")
            if (type != null && energyBudget != null && charge != null) {
                costs[type] = MobSpawnCost(energyBudget, charge)
            }
            cursor = nestedClose + 1
        }
        return costs
    }

    private fun objectBody(
        json: String,
        name: String,
    ): String? {
        val start = json.indexOf("\"$name\"")
        if (start < 0) return null
        val open = json.indexOf('{', start)
        if (open < 0) return null
        val end = matchingBrace(json, open) ?: return null
        return json.substring(open + 1, end)
    }

    private fun matchingBrace(
        text: String,
        open: Int,
    ): Int? {
        var depth = 0
        var end = open
        while (end < text.length) {
            when (text[end]) {
                '{' -> depth++
                '}' -> {
                    depth--
                    if (depth == 0) return end
                }
            }
            end++
        }
        return null
    }

    private fun entriesOf(body: String): List<SpawnerEntry> {
        val entries = mutableListOf<SpawnerEntry>()
        var cursor = 0
        while (true) {
            val open = body.indexOf('{', cursor)
            if (open < 0) break
            val close = body.indexOf('}', open)
            if (close < 0) break
            val fields = body.substring(open + 1, close)
            val type = stringField(fields, "type")
            val weight = intField(fields, "weight")
            val minCount = intField(fields, "minCount")
            val maxCount = intField(fields, "maxCount")
            if (type != null && weight != null && minCount != null && maxCount != null) {
                entries += SpawnerEntry(Identifier.parse(type), weight, minCount, maxCount)
            }
            cursor = close + 1
        }
        return entries
    }

    private fun stringField(
        body: String,
        name: String,
    ): String? {
        val at = body.indexOf("\"$name\"")
        if (at < 0) return null
        val open = body.indexOf('"', body.indexOf(':', at) + 1)
        if (open < 0) return null
        val close = body.indexOf('"', open + 1)
        if (close < 0) return null
        return body.substring(open + 1, close)
    }

    private fun intField(
        body: String,
        name: String,
    ): Int? {
        val at = body.indexOf("\"$name\"")
        if (at < 0) return null
        val colon = body.indexOf(':', at)
        if (colon < 0) return null
        val digits = body.drop(colon + 1).takeWhile { it.isDigit() || it == ' ' || it == '-' }.trim()
        return digits.toIntOrNull()
    }

    private fun doubleField(
        body: String,
        name: String,
    ): Double? {
        val at = body.indexOf("\"$name\"")
        if (at < 0) return null
        val colon = body.indexOf(':', at)
        if (colon < 0) return null
        val number =
            body
                .drop(colon + 1)
                .takeWhile {
                    it.isDigit() || it == ' ' || it == '-' || it == '+' || it == '.' || it == 'e' || it == 'E'
                }.trim()
        return number.toDoubleOrNull()
    }

    private fun creatureProbability(json: String): Float {
        val at = json.indexOf("\"creature_spawn_probability\"")
        if (at < 0) return DEFAULT_CREATURE_PROBABILITY
        val colon = json.indexOf(':', at)
        if (colon < 0) return DEFAULT_CREATURE_PROBABILITY
        val number = json.drop(colon + 1).takeWhile { it.isDigit() || it == ' ' || it == '-' || it == '.' }.trim()
        return number.toFloatOrNull() ?: DEFAULT_CREATURE_PROBABILITY
    }

    public companion object {
        private const val DEFAULT_CREATURE_PROBABILITY = 0.1f
        private val EMPTY_SETTINGS =
            BiomeSpawnSettings(emptyMap(), DEFAULT_CREATURE_PROBABILITY, emptyMap())

        /** The parser on its own, so it can be tested without a jar to read from. */
        internal fun parseForTest(json: String): Map<String, List<SpawnerEntry>> =
            RealBiomeSpawns(serverJar = null).parse(json)

        internal fun creatureProbabilityForTest(json: String): Float =
            RealBiomeSpawns(serverJar = null).creatureProbability(json)

        internal fun spawnCostsForTest(json: String): Map<Identifier, MobSpawnCost> =
            RealBiomeSpawns(serverJar = null).parseCosts(json)
    }
}
