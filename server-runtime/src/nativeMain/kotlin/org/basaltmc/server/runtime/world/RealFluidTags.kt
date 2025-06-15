package org.basaltmc.server.runtime.world

import org.basaltmc.server.assets.ServerJarAssets
import org.basaltmc.server.registry.tags.TagFile
import org.basaltmc.server.registry.tags.TagFileJson
import org.basaltmc.server.registry.tags.TagSet
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.logging.Logger

/**
 * The fluid tag graph out of the operator's own `server.jar`.
 *
 * The client asks these, not the block, before it will believe it is under water: `Camera
 * .getFluidInCamera` tests `FluidState.is(FluidTags.WATER)`, and `EntityFluidInteraction` keys its
 * trackers on the same two tags. Sent empty, as they were, water renders and nothing else about it
 * works: no fog, no overlay, no ambient sound, no air bar, no swimming.
 */
public class RealFluidTags(
    serverJar: ServerJarAssets?,
    private val logger: Logger = Logger("Tags"),
) {
    private val tags: Map<Identifier, Set<Identifier>> = serverJar?.let(::load).orEmpty()

    public val size: Int get() = tags.size

    public fun membersOf(tag: Identifier): Set<Identifier> = tags[tag].orEmpty()

    private fun load(serverJar: ServerJarAssets): Map<Identifier, Set<Identifier>> {
        val files = mutableMapOf<Identifier, TagFile>()
        for (entry in serverJar.entriesUnder(FLUID_TAG_PREFIX)) {
            if (!entry.endsWith(".json")) continue
            val path = entry.removePrefix(FLUID_TAG_PREFIX).removeSuffix(".json")
            runCatching { files[Identifier.vanilla(path)] = TagFileJson.parse(serverJar.readText(entry)) }
                .onFailure { logger.debug { "Skipped fluid tag $entry: ${it.message}" } }
        }
        return runCatching {
            TagSet.resolve(files) { it in PROTOCOL_IDS }
        }.getOrElse {
            logger.warn { "Fluid tags could not be resolved, so nothing counts as water: ${it.message}" }
            emptyMap()
        }
    }

    public companion object {
        private const val FLUID_TAG_PREFIX = "data/minecraft/tags/fluid/"

        /** parity: the `minecraft:fluid` registry, whose five ids are what a fluid tag carries. */
        public val PROTOCOL_IDS: Map<Identifier, Int> =
            mapOf(
                Identifier.vanilla("empty") to 0,
                Identifier.vanilla("flowing_water") to 1,
                Identifier.vanilla("water") to 2,
                Identifier.vanilla("flowing_lava") to 3,
                Identifier.vanilla("lava") to 4,
            )
    }
}
