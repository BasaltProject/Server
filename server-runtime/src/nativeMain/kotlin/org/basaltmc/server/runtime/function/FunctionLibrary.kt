package org.basaltmc.server.runtime.function

import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.logging.Logger
import org.basaltmc.wire.io.FileIO

/** One loaded function: its id and the command lines it runs, comments and blanks already dropped. */
public class ServerFunction(
    public val id: Identifier,
    public val lines: List<String>,
    /** Which datapack it came from, so `/datapack disable` can take it away again. */
    public val pack: String,
)

/**
 * Functions read from `datapacks/<pack>/data/<namespace>/function/<path>.mcfunction`.
 *
 * The layout is vanilla's, so a datapack written for the real server drops in unchanged. Only
 * functions are read: recipes, loot tables and advancements in the same pack are ignored, and the
 * pack's own `pack.mcmeta` is read for nothing but its existence, which is what marks a directory
 * as a pack at all.
 */
public class FunctionLibrary(
    private val root: String = DATAPACKS_ROOT,
    private val logger: Logger = Logger("Functions"),
) {
    private val functions = mutableMapOf<Identifier, ServerFunction>()
    private val disabledPacks = mutableSetOf<String>()

    /** Every pack directory found, whether or not it is enabled. */
    public var availablePacks: List<String> = emptyList()
        private set

    public val enabledPacks: List<String> get() = availablePacks.filter { it !in disabledPacks }

    public operator fun get(id: Identifier): ServerFunction? = functions[id]?.takeIf { it.pack !in disabledPacks }

    public fun all(): List<ServerFunction> = functions.values.filter { it.pack !in disabledPacks }

    public fun isEnabled(pack: String): Boolean = pack in availablePacks && pack !in disabledPacks

    public fun setEnabled(
        pack: String,
        enabled: Boolean,
    ): Boolean {
        if (pack !in availablePacks) return false
        val changed = if (enabled) disabledPacks.remove(pack) else disabledPacks.add(pack)
        return changed
    }

    /** Reads every pack again. Called at startup and by `/reload`, exactly as vanilla reloads packs. */
    public fun reload(): Int {
        functions.clear()
        availablePacks = if (FileIO.directoryExists(root)) FileIO.list(root).sorted() else emptyList()
        for (pack in availablePacks) {
            val dataRoot = "$root/$pack/data"
            if (!FileIO.directoryExists(dataRoot)) continue
            for (namespace in FileIO.list(dataRoot)) {
                val functionRoot = "$dataRoot/$namespace/function"
                if (!FileIO.directoryExists(functionRoot)) continue
                loadDirectory(pack, namespace, functionRoot, prefix = "")
            }
        }
        logger.info { "Loaded ${functions.size} function(s) from ${availablePacks.size} datapack(s)." }
        return functions.size
    }

    private fun loadDirectory(
        pack: String,
        namespace: String,
        directory: String,
        prefix: String,
    ) {
        for (entry in FileIO.list(directory)) {
            val path = "$directory/$entry"
            if (FileIO.directoryExists(path)) {
                loadDirectory(pack, namespace, path, "$prefix$entry/")
                continue
            }
            if (!entry.endsWith(FUNCTION_SUFFIX)) continue
            val text = FileIO.readText(path) ?: continue
            val id = Identifier(namespace, prefix + entry.removeSuffix(FUNCTION_SUFFIX))
            functions[id] =
                ServerFunction(
                    id,
                    text
                        .lineSequence()
                        .map { it.trim() }
                        .filter { it.isNotEmpty() && !it.startsWith("#") }
                        .toList(),
                    pack,
                )
        }
    }

    public companion object {
        /** Beside `worlds/` and `config/`, which is where an operator expects to drop a pack. */
        public const val DATAPACKS_ROOT: String = "datapacks"
        private const val FUNCTION_SUFFIX = ".mcfunction"
    }
}
