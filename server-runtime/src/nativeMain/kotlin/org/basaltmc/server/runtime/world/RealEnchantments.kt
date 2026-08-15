package org.basaltmc.server.runtime.world

import kotlinx.serialization.json.Json
import org.basaltmc.server.assets.ServerJarAssets
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.logging.Logger
import org.basaltmc.wire.nbt.JsonToNbt
import org.basaltmc.wire.nbt.NbtCompound

/**
 * The enchantments the operator's own `server.jar` ships, converted to the NBT the registry packet
 * carries.
 *
 * `minecraft:enchantment` is a datapack registry, so the client knows only what it was sent, and
 * the definitions are content: read from the download, never committed (§7).
 *
 * All or nothing. An entry the client cannot parse drops the connection at login, so one bad file
 * means an empty registry instead of a partial one.
 */
public class RealEnchantments(
    serverJar: ServerJarAssets?,
    private val logger: Logger = Logger("Enchantments"),
) {
    /** Id to definition, in sorted order: the index of an entry here is the id the wire carries. */
    public val entries: List<Pair<Identifier, NbtCompound>> = serverJar?.let(::load).orEmpty()

    private val indices: Map<Identifier, Int> = entries.withIndex().associate { (index, e) -> e.first to index }

    public val size: Int get() = entries.size

    /** Every name this registry holds, for the completions a command offers. */
    public val ids: List<Identifier> get() = entries.map { it.first }

    /** The registry index of an enchantment, which is what an item component references. */
    public fun protocolIdOf(id: Identifier): Int? = indices[id]

    /** Its definition as the jar wrote it, for the effects this server reads rather than forwards. */
    public fun definitionOf(id: Identifier): NbtCompound? = indices[id]?.let { entries[it].second }

    public operator fun contains(id: Identifier): Boolean = id in indices

    private fun load(serverJar: ServerJarAssets): List<Pair<Identifier, NbtCompound>> {
        val loaded = mutableListOf<Pair<Identifier, NbtCompound>>()
        for (entry in serverJar.entriesUnder(ENCHANTMENT_PREFIX).sorted()) {
            if (!entry.endsWith(".json")) continue
            val path = entry.removePrefix(ENCHANTMENT_PREFIX).removeSuffix(".json")
            val converted =
                runCatching {
                    JsonToNbt.convert(Json.parseToJsonElement(serverJar.readText(entry))) as NbtCompound
                }.getOrElse { error ->
                    logger.warn {
                        "Enchantment '$path' could not be converted (${error.message}); sending no enchantment " +
                            "registry at all, because a partial one would disconnect clients at login."
                    }
                    return emptyList()
                }
            loaded += Identifier.vanilla(path) to converted
        }
        if (loaded.isNotEmpty()) logger.info { "Loaded ${loaded.size} enchantment(s)." }
        return loaded
    }

    private companion object {
        const val ENCHANTMENT_PREFIX = "data/minecraft/enchantment/"
    }
}
