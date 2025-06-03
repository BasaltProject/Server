package org.basaltmc.server.runtime.config

import org.basaltmc.wire.common.logging.Logger
import org.basaltmc.wire.io.FileIO

/**
 * The `config/` directory: one file per domain, all TOML, all in one place. What makes Paper's
 * configuration hard is not the number of files but that they are split without a rule, in two
 * formats, with the same option in two places. A new file has to earn being its own domain rather
 * than one more table in an existing one.
 *
 * A missing file is generated from its commented default. A file that does not parse **stops the
 * server**: falling back to defaults would mean a typo in `groups.toml` quietly granting nobody
 * any permissions.
 */
public class ConfigDirectory(
    public val root: String = DEFAULT_ROOT,
    private val logger: Logger = Logger("Config"),
) {
    public fun pathOf(fileName: String): String = "$root/$fileName"

    /**
     * Reads one configuration file, writing [defaultContents] first if it is not there yet.
     *
     * [defaultContents] is expected to be the commented default: the file an operator opens on
     * first run should explain itself, not be an empty skeleton.
     */
    public fun load(
        fileName: String,
        defaultContents: String,
    ): TomlValue.Table {
        val path = pathOf(fileName)
        if (!FileIO.exists(path)) {
            FileIO.createDirectories(root)
            FileIO.writeText(path, defaultContents)
            logger.info { "$path not found, generating one with default values." }
        }
        val text = FileIO.readText(path) ?: throw TomlFormatException("Failed to read $path")
        return try {
            Toml.parse(text)
        } catch (e: TomlFormatException) {
            // Named rather than swallowed: with several files, "the configuration is broken" is not
            // a useful thing to tell someone.
            throw TomlFormatException("$path is not valid TOML: ${e.message}")
        }
    }

    public companion object {
        public const val DEFAULT_ROOT: String = "config"
    }
}
