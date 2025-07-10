package org.basaltmc.server.plugins

/** What the loader decided about one candidate. */
public sealed interface PluginOrdering {
    public data class Load(
        val manifest: PluginManifest,
    ) : PluginOrdering

    public data class Refused(
        val manifest: PluginManifest,
        val reason: String,
    ) : PluginOrdering
}

/**
 * Works out what loads and in what order.
 *
 * Deterministic: the same set of plugins gives the same order every time, with the ones that depend
 * on nothing going alphabetically. A server whose load order depends on the filesystem's mood is a
 * server whose bugs only happen on one machine.
 */
public object PluginOrder {
    public fun resolve(candidates: List<PluginManifest>): List<PluginOrdering> {
        val byId = candidates.associateBy { it.id }
        val refusals = LinkedHashMap<String, String>()

        for (manifest in candidates) {
            if (manifest.api != PluginManifest.API) {
                refusals[manifest.id] =
                    "it was written for API ${manifest.api} and this server speaks ${PluginManifest.API}"
            }
        }
        for (manifest in candidates) {
            if (manifest.id in refusals) continue
            for (dependency in manifest.hardDependencies) {
                val other = byId[dependency.id]
                when {
                    other == null -> refusals[manifest.id] = "it needs '${dependency.id}', which is not installed"
                    !dependency.accepts(other.version) ->
                        refusals[manifest.id] =
                            "it needs '${dependency.id}' ${dependency.requirement} and ${other.version} is installed"
                }
            }
        }

        // A plugin whose dependency was refused cannot load either, however deep the chain goes.
        var spread = true
        while (spread) {
            spread = false
            for (manifest in candidates) {
                if (manifest.id in refusals) continue
                val broken = manifest.hardDependencies.firstOrNull { it.id in refusals } ?: continue
                refusals[manifest.id] = "'${broken.id}' did not load"
                spread = true
            }
        }

        val ready = candidates.filter { it.id !in refusals }.sortedBy { it.id }
        val ordered = mutableListOf<PluginManifest>()
        val placed = mutableSetOf<String>()
        var progress = true
        while (progress) {
            progress = false
            for (manifest in ready) {
                if (manifest.id in placed) continue
                val waitingFor =
                    manifest.dependencies.filter { dependency ->
                        val present = dependency.id in ready.map { it.id }
                        present && dependency.id !in placed
                    }
                if (waitingFor.isNotEmpty()) continue
                ordered += manifest
                placed += manifest.id
                progress = true
            }
        }

        // Whatever is left is in a cycle: it is refused as a group, and the group is named, because
        // "one of these depends on itself" is not something an author can act on.
        val cycle = ready.filter { it.id !in placed }
        if (cycle.isNotEmpty()) {
            val names = cycle.map { it.id }.sorted().joinToString(", ")
            for (manifest in cycle) refusals[manifest.id] = "it is in a dependency cycle with: $names"
        }

        val out = mutableListOf<PluginOrdering>()
        for (manifest in ordered) out += PluginOrdering.Load(manifest)
        for (manifest in candidates.sortedBy { it.id }) {
            val reason = refusals[manifest.id] ?: continue
            out += PluginOrdering.Refused(manifest, reason)
        }
        return out
    }

    /** Who would break if [id] went away, furthest first, so unloading can start at the leaves. */
    public fun dependents(
        id: String,
        loaded: List<PluginManifest>,
    ): List<String> {
        val out = mutableListOf<String>()
        var frontier = listOf(id)
        while (frontier.isNotEmpty()) {
            val next =
                loaded
                    .filter { manifest -> manifest.hardDependencies.any { it.id in frontier } }
                    .map { it.id }
                    .filter { it !in out && it != id }
            if (next.isEmpty()) break
            out += next
            frontier = next
        }
        return out.reversed()
    }
}
