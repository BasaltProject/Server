package org.basaltmc.server.runtime.world

import org.basaltmc.server.assets.ServerJarAssets
import org.basaltmc.server.registry.tags.TagFile
import org.basaltmc.server.registry.tags.TagFileJson
import org.basaltmc.server.registry.tags.TagSet
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.logging.Logger

/**
 * The damage type tags out of the operator's own `server.jar`.
 *
 * They are what decides whether a hit goes through armour at all: starving, drowning, withering and
 * almost everything else this build can do to a player is in `bypasses_armor`, and reading that
 * from the jar is the difference between armour meaning something and armour meaning everything.
 */
public class RealDamageTypeTags(
    serverJar: ServerJarAssets?,
    private val logger: Logger = Logger("Tags"),
) {
    private val tags: Map<Identifier, Set<Identifier>> = serverJar?.let(::load).orEmpty()

    public val size: Int get() = tags.size

    public fun membersOf(tag: Identifier): Set<Identifier> = tags[tag].orEmpty()

    public fun isIn(
        tag: Identifier,
        damageType: Identifier,
    ): Boolean = damageType in tags[tag].orEmpty()

    private fun load(serverJar: ServerJarAssets): Map<Identifier, Set<Identifier>> {
        val files = mutableMapOf<Identifier, TagFile>()
        for (entry in serverJar.entriesUnder(PREFIX)) {
            if (!entry.endsWith(".json")) continue
            val path = entry.removePrefix(PREFIX).removeSuffix(".json")
            runCatching { files[Identifier.vanilla(path)] = TagFileJson.parse(serverJar.readText(entry)) }
                .onFailure { logger.debug { "Skipped damage type tag $entry: ${it.message}" } }
        }
        // Damage types are not a registry this build models, so every id a tag names is taken as
        // real: there is nothing to check it against, and dropping one would silently lose a rule.
        return runCatching { TagSet.resolve(files) { true } }.getOrElse {
            logger.warn { "Damage type tags could not be resolved, so armour stops everything: ${it.message}" }
            emptyMap()
        }
    }

    public companion object {
        private const val PREFIX = "data/minecraft/tags/damage_type/"

        public val BYPASSES_ARMOUR: Identifier = Identifier.vanilla("bypasses_armor")
        public val BYPASSES_EFFECTS: Identifier = Identifier.vanilla("bypasses_effects")
        public val BYPASSES_RESISTANCE: Identifier = Identifier.vanilla("bypasses_resistance")
        public val BYPASSES_ENCHANTMENTS: Identifier = Identifier.vanilla("bypasses_enchantments")
    }
}

/** The damage types this build can deal, by the names the jar's own tags use. */
public object DamageTypes {
    public val OUTSIDE_BORDER: Identifier = Identifier.vanilla("outside_border")
    public val DROWN: Identifier = Identifier.vanilla("drown")
    public val LAVA: Identifier = Identifier.vanilla("lava")
    public val ON_FIRE: Identifier = Identifier.vanilla("on_fire")
    public val MAGIC: Identifier = Identifier.vanilla("magic")
    public val WITHER: Identifier = Identifier.vanilla("wither")
    public val STARVE: Identifier = Identifier.vanilla("starve")
}
