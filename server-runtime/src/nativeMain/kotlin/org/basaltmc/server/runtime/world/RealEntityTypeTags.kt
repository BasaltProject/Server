package org.basaltmc.server.runtime.world

import org.basaltmc.server.assets.ServerJarAssets
import org.basaltmc.server.registry.EntityTypes
import org.basaltmc.server.registry.tags.TagFile
import org.basaltmc.server.registry.tags.TagFileJson
import org.basaltmc.server.registry.tags.TagSet
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.logging.Logger

/**
 * The entity type tag graph out of the operator's own `server.jar`, flattened once at startup.
 *
 * What `@e[type=#minecraft:skeletons]` resolves through. Datapack content, so it is read from the
 * download rather than committed (§7), the same as block and item tags.
 */
public class RealEntityTypeTags(
    serverJar: ServerJarAssets?,
    private val logger: Logger = Logger("Tags"),
) {
    private val tags: Map<Identifier, Set<Identifier>> = serverJar?.let(::load).orEmpty()

    public val size: Int get() = tags.size

    /** Everything in a tag, or empty for one nobody declared. */
    public fun membersOf(tag: Identifier): Set<Identifier> = tags[tag].orEmpty()

    private fun load(serverJar: ServerJarAssets): Map<Identifier, Set<Identifier>> {
        val files = mutableMapOf<Identifier, TagFile>()
        for (entry in serverJar.entriesUnder(ENTITY_TAG_PREFIX)) {
            if (!entry.endsWith(".json")) continue
            val path = entry.removePrefix(ENTITY_TAG_PREFIX).removeSuffix(".json")
            runCatching { files[Identifier.vanilla(path)] = TagFileJson.parse(serverJar.readText(entry)) }
                .onFailure { logger.debug { "Skipped entity type tag $entry: ${it.message}" } }
        }
        return runCatching {
            TagSet.resolve(files) { EntityTypes[it] != null }
        }.getOrElse {
            logger.warn {
                "Entity type tags could not be resolved, so `type=#tag` selectors match nobody: ${it.message}"
            }
            emptyMap()
        }
    }

    private companion object {
        const val ENTITY_TAG_PREFIX = "data/minecraft/tags/entity_type/"
    }
}
