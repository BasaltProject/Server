package org.basaltmc.server.config

import org.basaltmc.common.logging.Logger
import org.basaltmc.io.FileIO

/**
 * The `config/` directory: one file per domain, all TOML, all in one place.
 *
 * This replaces the earlier rule of a single configuration file. That rule existed against the sprawl of
 * Paper (`server.properties` plus `bukkit.yml` plus `spigot.yml` plus two `paper-*.yml` plus a
 * folder), and the goal has not changed, only the diagnosis: what makes Paper's configuration hard
 * is that it is scattered *without a rule*, in two formats, half of it in the root, with the same
 * option in two places. One file stops working as soon as permission groups, world creation, list
 * profiles and per-command settings arrive, because it becomes thousands of lines nobody can find
 * anything in.
 *
 * So: one directory, one file per domain, and adding a file has to be justified by being a domain
 * of its own rather than one more table in an existing file.
 *
 * A file that does not exist is generated from its commented default. A file that exists but does
 * not parse **stops the server**, on purpose: silently falling back to defaults would mean a
 * `groups.toml` with a typo quietly granting nobody any permissions.
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
            logger.info { "$path not found, generating one with vanilla-parity defaults." }
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
