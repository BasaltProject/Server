package org.basaltmc.server.runtime.world

import kotlinx.serialization.json.Json
import org.basaltmc.server.assets.ServerJarAssets
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.logging.Logger
import org.basaltmc.wire.nbt.JsonToNbt
import org.basaltmc.wire.nbt.NbtCompound

/**
 * The dialogs the operator's own `server.jar` ships, as the NBT the registry packet carries.
 *
 * Same shape and the same all-or-nothing rule as [RealEnchantments]: `minecraft:dialog` is a
 * datapack registry, a client knows only what it was sent, and an entry it cannot parse drops the
 * connection at login.
 */
public class RealDialogs(
    serverJar: ServerJarAssets?,
    private val logger: Logger = Logger("Dialogs"),
) {
    public val entries: List<Pair<Identifier, NbtCompound>> = serverJar?.let(::load).orEmpty()

    private val indices: Map<Identifier, Int> = entries.withIndex().associate { (index, e) -> e.first to index }

    public val size: Int get() = entries.size

    public fun protocolIdOf(id: Identifier): Int? = indices[id]

    public operator fun contains(id: Identifier): Boolean = id in indices

    private fun load(serverJar: ServerJarAssets): List<Pair<Identifier, NbtCompound>> {
        val loaded = mutableListOf<Pair<Identifier, NbtCompound>>()
        for (entry in serverJar.entriesUnder(DIALOG_PREFIX).sorted()) {
            if (!entry.endsWith(".json")) continue
            val path = entry.removePrefix(DIALOG_PREFIX).removeSuffix(".json")
            val converted =
                runCatching {
                    JsonToNbt.convert(Json.parseToJsonElement(serverJar.readText(entry))) as NbtCompound
                }.getOrElse { error ->
                    logger.warn {
                        "Dialog '$path' could not be converted (${error.message}); sending no dialog registry, " +
                            "because a partial one would disconnect clients at login."
                    }
                    return emptyList()
                }
            loaded += Identifier.vanilla(path) to converted
        }
        if (loaded.isNotEmpty()) logger.info { "Loaded ${loaded.size} dialog(s)." }
        return loaded
    }

    private companion object {
        const val DIALOG_PREFIX = "data/minecraft/dialog/"
    }
}
