package org.basaltmc.server.storage

import org.basaltmc.server.storage.db.PlayerDatabase

/**
 * What plugins keep about players, one namespace each.
 *
 * Every call takes the plugin's id and every query filters on it, which is the whole of the
 * isolation: there is no way to ask this for somebody else's rows, because there is no call that
 * does not name your own. The values are text because a plugin's own data has no schema the server
 * could enforce; what it does enforce is whose data it is.
 */
public class PluginDataRepository(
    private val database: PlayerDatabase,
) {
    public fun get(
        plugin: String,
        uuid: String,
        key: String,
    ): String? = database.pluginDataQueries.selectValue(plugin, uuid, key).executeAsOneOrNull()

    public fun all(
        plugin: String,
        uuid: String,
    ): Map<String, String> =
        database.pluginDataQueries
            .selectAllOf(plugin, uuid)
            .executeAsList()
            .associate { it.key to it.value_ }

    public fun set(
        plugin: String,
        uuid: String,
        key: String,
        value: String?,
    ) {
        if (value == null) {
            database.pluginDataQueries.deleteValue(plugin, uuid, key)
        } else {
            database.pluginDataQueries.setValue(plugin, uuid, key, value)
        }
    }

    /** The query a file cannot answer: the highest [key] across every player this plugin knows. */
    public fun top(
        plugin: String,
        key: String,
        limit: Int,
    ): List<Pair<String, String>> =
        database.pluginDataQueries
            .selectTop(plugin, key, limit.toLong())
            .executeAsList()
            .map { it.uuid to it.value_ }

    /** Which plugins have rows here, and how many, for the orphan check at startup. */
    public fun namespaces(): Map<String, Long> =
        database.pluginDataQueries
            .selectPlugins()
            .executeAsList()
            .associate { it.plugin to it.rows }

    /** What `/plugins purge` removes. Nothing else deletes a plugin's data, ever. */
    public fun purge(plugin: String) {
        database.pluginDataQueries.deletePlugin(plugin)
    }
}
