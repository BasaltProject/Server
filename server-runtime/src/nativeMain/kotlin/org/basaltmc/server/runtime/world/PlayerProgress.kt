package org.basaltmc.server.runtime.world

import org.basaltmc.server.assets.ServerJarAssets
import org.basaltmc.server.storage.ProgressRepository
import org.basaltmc.server.world.advancement.Advancement
import org.basaltmc.server.world.advancement.AdvancementJson
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.logging.Logger

/**
 * Every advancement the operator's own `server.jar` ships, by id.
 *
 * Datapack content, so it is read from the download rather than committed (§7), the same way loot
 * tables and recipes are. What awards them is [AdvancementTriggers]; `/advancement grant` remains
 * the only way to complete one whose trigger this build cannot observe yet.
 */
public class RealAdvancements(
    serverJar: ServerJarAssets?,
    private val logger: Logger = Logger("Advancements"),
    /** Definitions handed in directly, which is how a test builds one without a jar to read. */
    definitions: Map<Identifier, Advancement>? = null,
) {
    private val advancements: Map<Identifier, Advancement> = definitions ?: serverJar?.let(::load).orEmpty()

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
 * What each player has been granted or taught, kept in memory and written through to `players.db`.
 *
 * Keyed by UUID rather than by name, which is both what the database stores and what survives a
 * rename: vanilla keys `advancements/<uuid>.json` the same way. [load] fills a player in at join;
 * everything else answers from memory, so a lookup on the crafting path costs no query.
 *
 * The repository is null where there is no database, which is how the command tests run: the whole
 * class then behaves as it did before persistence existed.
 */
public class PlayerProgress(
    private val repository: ProgressRepository? = null,
) {
    private val advancements = mutableMapOf<String, MutableMap<Identifier, MutableSet<String>>>()
    private val recipes = mutableMapOf<String, MutableSet<Identifier>>()

    /** Reads one player's rows in. Called at join, before anything can ask about them. */
    public fun load(uuid: String) {
        val stored = repository?.load(uuid) ?: return
        advancements[uuid] =
            stored.criteria
                .mapKeys { (id, _) -> Identifier.parse(id) }
                .mapValues { (_, names) -> names.toMutableSet() }
                .toMutableMap()
        recipes[uuid] = stored.recipes.map(Identifier::parse).toMutableSet()
    }

    /** Drops a player from memory at quit. Their rows stay in the database. */
    public fun unload(uuid: String) {
        advancements.remove(uuid)
        recipes.remove(uuid)
    }

    /** Marks one criterion as done. Returns false when it already was. */
    public fun grantCriterion(
        uuid: String,
        advancement: Identifier,
        criterion: String,
    ): Boolean {
        val added =
            advancements
                .getOrPut(
                    uuid,
                ) { mutableMapOf() }
                .getOrPut(advancement) { mutableSetOf() }
                .add(criterion)
        if (added) repository?.grantCriterion(uuid, advancement.toString(), criterion)
        return added
    }

    public fun revokeCriterion(
        uuid: String,
        advancement: Identifier,
        criterion: String,
    ): Boolean {
        val removed = advancements[uuid]?.get(advancement)?.remove(criterion) == true
        if (removed) repository?.revokeCriterion(uuid, advancement.toString(), criterion)
        return removed
    }

    public fun grantedCriteria(
        uuid: String,
        advancement: Identifier,
    ): Set<String> = advancements[uuid]?.get(advancement).orEmpty()

    public fun revokeAll(
        uuid: String,
        advancement: Identifier,
    ): Boolean {
        val removed = advancements[uuid]?.remove(advancement) != null
        if (removed) repository?.revokeAdvancement(uuid, advancement.toString())
        return removed
    }

    public fun knownRecipes(uuid: String): Set<Identifier> = recipes[uuid].orEmpty()

    public fun learn(
        uuid: String,
        recipe: Identifier,
    ): Boolean {
        val added = recipes.getOrPut(uuid) { mutableSetOf() }.add(recipe)
        if (added) repository?.learnRecipe(uuid, recipe.toString())
        return added
    }

    public fun forget(
        uuid: String,
        recipe: Identifier,
    ): Boolean {
        val removed = recipes[uuid]?.remove(recipe) == true
        if (removed) repository?.forgetRecipe(uuid, recipe.toString())
        return removed
    }
}
