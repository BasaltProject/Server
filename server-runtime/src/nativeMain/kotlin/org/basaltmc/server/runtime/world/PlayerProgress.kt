package org.basaltmc.server.runtime.world

import org.basaltmc.server.assets.ServerJarAssets
import org.basaltmc.server.world.advancement.Advancement
import org.basaltmc.server.world.advancement.AdvancementJson
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.logging.Logger

/**
 * Every advancement the operator's own `server.jar` ships, by id.
 *
 * Datapack content, so it is read from the download rather than committed (§7), the same way loot
 * tables and recipes are. Nothing *awards* them yet: the triggers are gameplay events that do not
 * exist, which is exactly why `/advancement grant` is the way one is completed for now.
 */
public class RealAdvancements(
    serverJar: ServerJarAssets?,
    private val logger: Logger = Logger("Advancements"),
) {
    private val advancements: Map<Identifier, Advancement> = serverJar?.let(::load).orEmpty()

    public val size: Int get() = advancements.size

    public val ids: Set<Identifier> get() = advancements.keys

    public operator fun get(id: Identifier): Advancement? = advancements[id]

    /** [id] and everything below it, which is what `/advancement grant ... from` acts on. */
    public fun descendantsOf(id: Identifier): List<Identifier> {
        val result = mutableListOf(id)
        var frontier = listOf(id)
        while (frontier.isNotEmpty()) {
            val children = advancements.filterValues { it.parent in frontier }.keys
            if (children.isEmpty()) break
            result += children
            frontier = children.toList()
        }
        return result
    }

    /** Everything from the root down to [id], which is `/advancement grant ... until`. */
    public fun ancestorsOf(id: Identifier): List<Identifier> {
        val result = mutableListOf(id)
        var parent = advancements[id]?.parent
        while (parent != null) {
            result += parent
            parent = advancements[parent]?.parent
        }
        return result
    }

    private fun load(serverJar: ServerJarAssets): Map<Identifier, Advancement> {
        val loaded = mutableMapOf<Identifier, Advancement>()
        for (entry in serverJar.entriesUnder(ADVANCEMENT_PREFIX)) {
            if (!entry.endsWith(".json")) continue
            val path = entry.removePrefix(ADVANCEMENT_PREFIX).removeSuffix(".json")
            runCatching { AdvancementJson.parse(serverJar.readText(entry)) }
                .onSuccess { loaded[Identifier.vanilla(path)] = it }
                .onFailure { logger.debug { "Skipped advancement $entry: ${it.message}" } }
        }
        logger.info { "Loaded ${loaded.size} advancement(s)." }
        return loaded
    }

    private companion object {
        const val ADVANCEMENT_PREFIX = "data/minecraft/advancement/"
    }
}

/**
 * What each player has been granted or taught, in memory for the run.
 *
 * Vanilla keeps both in the world folder (`advancements/<uuid>.json`, and the recipe book inside
 * the player's own data). Here they are not persisted yet, which is stated rather than implied: a
 * `/advancement grant` survives until the server stops.
 */
public class PlayerProgress {
    private val advancements = mutableMapOf<String, MutableMap<Identifier, MutableSet<String>>>()
    private val recipes = mutableMapOf<String, MutableSet<Identifier>>()

    /** Marks one criterion as done. Returns false when it already was. */
    public fun grantCriterion(
        player: String,
        advancement: Identifier,
        criterion: String,
    ): Boolean =
        advancements.getOrPut(player) { mutableMapOf() }.getOrPut(advancement) { mutableSetOf() }.add(criterion)

    public fun revokeCriterion(
        player: String,
        advancement: Identifier,
        criterion: String,
    ): Boolean = advancements[player]?.get(advancement)?.remove(criterion) == true

    public fun grantedCriteria(
        player: String,
        advancement: Identifier,
    ): Set<String> = advancements[player]?.get(advancement).orEmpty()

    public fun revokeAll(
        player: String,
        advancement: Identifier,
    ): Boolean = advancements[player]?.remove(advancement) != null

    public fun knownRecipes(player: String): Set<Identifier> = recipes[player].orEmpty()

    public fun learn(
        player: String,
        recipe: Identifier,
    ): Boolean = recipes.getOrPut(player) { mutableSetOf() }.add(recipe)

    public fun forget(
        player: String,
        recipe: Identifier,
    ): Boolean = recipes[player]?.remove(recipe) == true
}
