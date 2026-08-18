package org.basaltmc.server.runtime.world

import org.basaltmc.server.assets.ServerJarAssets
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.logging.Logger
import kotlin.concurrent.Volatile

/** One trade as the jar declares it: what it wants, what it gives, and how often. */
public class VillagerTrade(
    public val id: Identifier,
    public val wants: Identifier,
    public val wantsCount: Int,
    public val alsoWants: Identifier?,
    public val alsoWantsCount: Int,
    public val gives: Identifier,
    public val givesCount: Int,
    public val maxUses: Int,
    public val experience: Int,
    public val priceMultiplier: Float,
)

/**
 * Every villager trade the operator's own `server.jar` ships, by profession and level.
 *
 * parity: 26.2 moved the trades out of `VillagerTrades` and into data: each one is a file under
 * `data/minecraft/villager_trade/<profession>/<level>/`, and the tag
 * `data/minecraft/tags/villager_trade/<profession>/level_<n>.json` lists which belong to that level.
 * Datapack content, so it is read from the download rather than committed.
 */
public class RealVillagerTrades(
    private val serverJar: ServerJarAssets?,
    private val logger: Logger = Logger("VillagerTrades"),
) {
    @Volatile
    private var cache: Map<String, List<VillagerTrade>> = emptyMap()

    /** The trades a villager of this profession offers at this level, in the order the tag lists them. */
    public fun forProfession(
        profession: String,
        level: Int,
    ): List<VillagerTrade> {
        val key = "$profession/$level"
        cache[key]?.let { return it }
        val jar = serverJar ?: return emptyList()
        val loaded =
            runCatching {
                val tag = jar.readText("data/minecraft/tags/villager_trade/$profession/level_$level.json")
                idsOf(tag).mapNotNull { id -> read(jar, id) }
            }.onFailure { logger.debug { "No trades for $profession level $level: ${it.message}" } }
                .getOrDefault(emptyList())
        cache = cache + (key to loaded)
        return loaded
    }

    private fun read(
        jar: ServerJarAssets,
        id: Identifier,
    ): VillagerTrade? =
        runCatching {
            val json = jar.readText("data/minecraft/villager_trade/${id.path}.json")
            parse(id, json)
        }.getOrNull()

    /** parity: the `values` of a tag file, as ids. */
    private fun idsOf(json: String): List<Identifier> {
        val values = json.substringAfter("\"values\"", "").substringAfter('[', "").substringBefore(']')
        return values
            .split(',')
            .mapNotNull { it.trim().removeSurrounding("\"").takeIf { name -> name.isNotEmpty() } }
            .map { Identifier.parse(it) }
    }

    /**
     * Reads one trade file. The fields are flat and few, so this reads them by name rather than
     * modelling the whole document: what a villager wants, what it gives, and the three numbers.
     */
    private fun parse(
        id: Identifier,
        json: String,
    ): VillagerTrade? {
        val wants = objectAt(json, "wants") ?: return null
        val gives = objectAt(json, "gives") ?: return null
        val extra = objectAt(json, "additional_wants")
        return VillagerTrade(
            id = id,
            wants = Identifier.parse(stringField(wants, "id") ?: return null),
            wantsCount = numberField(wants, "count")?.toInt() ?: 1,
            alsoWants = extra?.let { stringField(it, "id") }?.let { Identifier.parse(it) },
            alsoWantsCount = extra?.let { numberField(it, "count")?.toInt() } ?: 0,
            gives = Identifier.parse(stringField(gives, "id") ?: return null),
            givesCount = numberField(gives, "count")?.toInt() ?: 1,
            maxUses = numberField(json, "max_uses")?.toInt() ?: DEFAULT_MAX_USES,
            experience = numberField(json, "xp")?.toInt() ?: 0,
            priceMultiplier = numberField(json, "reputation_discount")?.toFloat() ?: 0f,
        )
    }

    /** The braces of one named object, so its own fields can be read without the rest of the file. */
    private fun objectAt(
        json: String,
        name: String,
    ): String? {
        val at = json.indexOf("\"$name\"")
        if (at < 0) return null
        val open = json.indexOf('{', at)
        if (open < 0) return null
        var depth = 0
        var index = open
        while (index < json.length) {
            when (json[index]) {
                '{' -> depth++
                '}' -> {
                    depth--
                    if (depth == 0) return json.substring(open + 1, index)
                }
            }
            index++
        }
        return null
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

    private fun numberField(
        body: String,
        name: String,
    ): Double? {
        val at = body.indexOf("\"$name\"")
        if (at < 0) return null
        val colon = body.indexOf(':', at)
        if (colon < 0) return null
        val digits =
            body
                .drop(colon + 1)
                .takeWhile { it.isDigit() || it == '.' || it == '-' || it == ' ' }
                .trim()
        return digits.toDoubleOrNull()
    }

    public companion object {
        /** parity: `MerchantOffer`'s own default when a trade file does not say. */
        public const val DEFAULT_MAX_USES: Int = 12

        /** The parser on its own, so it can be tested without a jar to read from. */
        internal fun parseForTest(
            id: Identifier,
            json: String,
        ): VillagerTrade? = RealVillagerTrades(serverJar = null).parse(id, json)
    }
}
