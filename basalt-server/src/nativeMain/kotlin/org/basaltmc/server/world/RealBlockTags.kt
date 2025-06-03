package org.basaltmc.server.world

import org.basaltmc.assets.ServerJarAssets
import org.basaltmc.common.Identifier
import org.basaltmc.common.logging.Logger
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.tags.TagFile
import org.basaltmc.registry.tags.TagFileJson
import org.basaltmc.registry.tags.TagSet

/**
 * The block tag graph out of the operator's own `server.jar`, flattened once at startup.
 *
 * This is what turns a tool's rules into an answer: a rule names `minecraft:mineable/pickaxe` or
 * `minecraft:incorrect_for_diamond_tool`, and only the real tag files say which blocks are in them.
 * Datapack content, so it is read from the download rather than committed (§7), the same as loot
 * tables and recipes.
 */
public class RealBlockTags(
    serverJar: ServerJarAssets?,
    private val logger: Logger = Logger("Tags"),
) {
    private val tags: Map<Identifier, Set<Identifier>> = serverJar?.let(::load).orEmpty()

    public val size: Int get() = tags.size

    /** Everything in a tag, by the tag's full `namespace:path` name. Empty for one that is unknown. */
    public fun membersOf(tag: String): Set<Identifier> = tags[Identifier.parse(tag)].orEmpty()

    /** Whether a block is in a tag, by the tag's full `namespace:path` name. */
    public fun contains(
        tag: String,
        block: Identifier,
    ): Boolean = tags[Identifier.parse(tag)]?.contains(block) == true

    private fun load(serverJar: ServerJarAssets): Map<Identifier, Set<Identifier>> {
        val files = mutableMapOf<Identifier, TagFile>()
        for (entry in serverJar.entriesUnder(BLOCK_TAG_PREFIX)) {
            if (!entry.endsWith(".json")) continue
            val path = entry.removePrefix(BLOCK_TAG_PREFIX).removeSuffix(".json")
            runCatching { files[Identifier.vanilla(path)] = TagFileJson.parse(serverJar.readText(entry)) }
                .onFailure { logger.debug { "Skipped block tag $entry: ${it.message}" } }
        }
        return runCatching {
            TagSet.resolve(files) { Blocks.REGISTRY.byIdentifier(it) != null }
        }.getOrElse {
            logger.warn { "Block tags could not be resolved, so tool requirements will not apply: ${it.message}" }
            emptyMap()
        }
    }

    private companion object {
        const val BLOCK_TAG_PREFIX = "data/minecraft/tags/block/"
    }
}
