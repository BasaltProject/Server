package org.basaltmc.server.world

import org.basaltmc.assets.ServerJarAssets
import org.basaltmc.common.Identifier
import org.basaltmc.common.logging.Logger
import org.basaltmc.world.loot.LootTable
import org.basaltmc.world.loot.LootTableJson
import org.basaltmc.world.loot.LootTableParseException
import kotlin.concurrent.Volatile

/**
 * Resolves a block's break drops from the real `data/minecraft/loot_table/blocks/<path>.json` the
 * operator's own downloaded [serverJar] carries (never embedded in this repo, see
 * [ServerJarAssets]'s own KDoc, matching [RealNoiseParameters]'s own pattern for worldgen JSON).
 *
 * Results are cached, including `null` (no table found, or [LootTableJson] cannot parse this one
 * yet): a table that throws once is not worth re-parsing on every single block broken.
 */
public class RealLootTables(
    private val serverJar: ServerJarAssets?,
    private val logger: Logger = Logger("Loot"),
) {
    // Copy-on-write, same reasoning as RealNoiseParameters: many connection threads can break a
    // block at once, and a MutableMap written from several of them at the same time corrupts its
    // own backing array.
    @Volatile
    private var cache: Map<Identifier, LootTable?> = emptyMap()

    /** The block's own loot table, or `null` if it breaks empty-handed (no table, or one this build cannot parse yet). */
    public fun forBlock(block: Identifier): LootTable? {
        cache[block]?.let { return it }
        if (cache.containsKey(block)) return null
        val jar = serverJar ?: return null

        val resolved =
            runCatching {
                LootTableJson.parse(jar.readText("data/minecraft/loot_table/blocks/${block.path}.json"))
            }.onFailure { error ->
                val reason = if (error is LootTableParseException) error.message else "no such loot table"
                logger.debug { "$block drops nothing: $reason" }
            }.getOrNull()

        cache = cache + (block to resolved)
        return resolved
    }
}
