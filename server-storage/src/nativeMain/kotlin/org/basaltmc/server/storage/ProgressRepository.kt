package org.basaltmc.server.storage

import org.basaltmc.server.storage.db.PlayerDatabase

/** One player's granted criteria, by advancement id, and the recipes in their book. */
public data class StoredProgress(
    public val criteria: Map<String, Set<String>>,
    public val recipes: Set<String>,
)

/**
 * Advancement criteria and recipe books, written as they change rather than at save time.
 *
 * A grant is one row, so there is nothing to lose to a crash between the command and the next save,
 * which is the failure `advancements/<uuid>.json` has: vanilla rewrites the whole file and a
 * truncated one is the whole player's progress.
 */
public class ProgressRepository(
    database: PlayerDatabase,
) {
    private val queries = database.progressQueries

    public fun load(uuid: String): StoredProgress {
        val criteria = mutableMapOf<String, MutableSet<String>>()
        for (row in queries.selectCriteria(uuid).executeAsList()) {
            criteria.getOrPut(row.advancement) { mutableSetOf() } += row.criterion
        }
        return StoredProgress(criteria, queries.selectRecipes(uuid).executeAsList().toSet())
    }

    public fun grantCriterion(
        uuid: String,
        advancement: String,
        criterion: String,
    ) {
        queries.grantCriterion(uuid, advancement, criterion)
    }

    public fun revokeCriterion(
        uuid: String,
        advancement: String,
        criterion: String,
    ) {
        queries.revokeCriterion(uuid, advancement, criterion)
    }

    public fun revokeAdvancement(
        uuid: String,
        advancement: String,
    ) {
        queries.revokeAdvancement(uuid, advancement)
    }

    public fun learnRecipe(
        uuid: String,
        recipe: String,
    ) {
        queries.learnRecipe(uuid, recipe)
    }

    public fun forgetRecipe(
        uuid: String,
        recipe: String,
    ) {
        queries.forgetRecipe(uuid, recipe)
    }
}
