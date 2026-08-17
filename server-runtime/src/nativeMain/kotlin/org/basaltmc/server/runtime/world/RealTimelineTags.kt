package org.basaltmc.server.runtime.world

import org.basaltmc.server.assets.ServerJarAssets
import org.basaltmc.server.registry.tags.TagFile
import org.basaltmc.server.registry.tags.TagFileJson
import org.basaltmc.server.registry.tags.TagSet
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.logging.Logger

/**
 * The timeline tag graph out of the operator's own `server.jar`.
 *
 * A dimension does not name its timelines one by one: it names a tag of them (`#minecraft:in_over
 * world`), and the client draws the sky from whatever that tag resolves to. Sent empty, as it was,
 * the clock ticks and nothing reads it, which is exactly what a fixed midday sky looks like.
 */
public class RealTimelineTags(
    serverJar: ServerJarAssets?,
    private val logger: Logger = Logger("Tags"),
) {
    private val tags: Map<Identifier, Set<Identifier>> = serverJar?.let(::load).orEmpty()

    public val size: Int get() = tags.size

    public fun membersOf(tag: Identifier): Set<Identifier> = tags[tag].orEmpty()

    private fun load(serverJar: ServerJarAssets): Map<Identifier, Set<Identifier>> {
        val files = mutableMapOf<Identifier, TagFile>()
        for (entry in serverJar.entriesUnder(PREFIX)) {
            if (!entry.endsWith(".json")) continue
            val path = entry.removePrefix(PREFIX).removeSuffix(".json")
            runCatching { files[Identifier.vanilla(path)] = TagFileJson.parse(serverJar.readText(entry)) }
                .onFailure { logger.debug { "Skipped timeline tag $entry: ${it.message}" } }
        }
        // Every id a tag names is taken as real: the timeline registry is whatever the jar ships,
        // and this build sends it verbatim.
        return runCatching { TagSet.resolve(files) { true } }.getOrElse {
            logger.warn { "Timeline tags could not be resolved, so the sky will not move: ${it.message}" }
            emptyMap()
        }
    }

    private companion object {
        const val PREFIX = "data/minecraft/tags/timeline/"
    }
}
