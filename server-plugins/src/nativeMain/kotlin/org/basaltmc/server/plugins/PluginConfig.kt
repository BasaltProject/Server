package org.basaltmc.server.plugins

import org.basaltmc.wire.common.toml.Toml
import org.basaltmc.wire.common.toml.TomlFormatException
import org.basaltmc.wire.common.toml.TomlValue
import org.basaltmc.wire.io.FileIO

/**
 * A plugin's configuration: whichever TOML files it has, wherever it put them.
 *
 * Addressed by name and not by place, so `modules/chat.toml` is `config.chat` and moving it does not
 * change a line of Lua. A `config.toml` at the top is the exception: its keys go straight on
 * `config`. Which is why two files cannot share a name: see [PluginFolder.index].
 */
public class PluginConfig {
    public fun load(files: Map<String, String>): TomlValue.Table {
        val out = TomlValue.Table()
        for ((relative, path) in files.entries.sortedBy { it.key }) {
            if (!relative.endsWith(".toml")) continue
            val text = FileIO.readText(path) ?: continue
            val parsed =
                try {
                    Toml.parse(text)
                } catch (e: TomlFormatException) {
                    throw PluginLoadException("$path is malformed: ${e.message}")
                }
            if (relative == ROOT_FILE) {
                for (key in parsed.keys()) parsed[key]?.let { out[key] = it }
            } else {
                out[relative.substringAfterLast('/').removeSuffix(".toml")] = parsed
            }
        }
        return out
    }

    private companion object {
        const val ROOT_FILE = "config.toml"
    }
}
