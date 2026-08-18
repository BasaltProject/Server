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
    private var cache: Map<String, Map<String, List<SpawnerEntry>>> = emptyMap()

    /** The spawn list of [biome] for [category], empty when the biome offers nothing of that kind. */
    public fun listFor(
        biome: Identifier,
        category: String,
    ): List<SpawnerEntry> = of(biome)[category].orEmpty()

    public fun of(biome: Identifier): Map<String, List<SpawnerEntry>> {
        val key = biome.path
        cache[key]?.let { return it }
        val jar = serverJar ?: return emptyMap()
        val parsed =
            runCatching { parse(jar.readText("data/minecraft/worldgen/biome/$key.json")) }
                .onFailure { logger.debug { "No spawn list for biome '$key': ${it.message}" } }
                .getOrDefault(emptyMap())
        cache = cache + (key to parsed)
        return parsed
    }

    /**
     * Reads the `spawners` object without a full JSON model: it is a map of category to a list of
     * flat objects, and everything else in a biome file is worldgen's business rather than this one's.
     */
    private fun parse(json: String): Map<String, List<SpawnerEntry>> {
        val start = json.indexOf("\"spawners\"")
        if (start < 0) return emptyMap()
        val open = json.indexOf('{', start)
        if (open < 0) return emptyMap()
        var depth = 0
        var end = open
        while (end < json.length) {
            when (json[end]) {
                '{' -> depth++
                '}' -> {
                    depth--
                    if (depth == 0) break
                }
            }
            end++
        }
        val body = json.substring(open + 1, end)
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

    public companion object {
        /** The parser on its own, so it can be tested without a jar to read from. */
        internal fun parseForTest(json: String): Map<String, List<SpawnerEntry>> =
            RealBiomeSpawns(serverJar = null).parse(json)
    }
}
