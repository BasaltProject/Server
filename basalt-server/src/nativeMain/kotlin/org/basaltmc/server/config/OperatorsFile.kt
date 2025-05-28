package org.basaltmc.server.config

import org.basaltmc.common.logging.Logger
import org.basaltmc.io.FileIO

/**
 * Writes the operator list back into `basalt.toml`, so `op` and `deop` outlive a restart.
 *
 * Only the `operators = [...]` line is touched; everything else in the file, comments included,
 * is left byte for byte as it was. That is why this rewrites a line instead of re-serializing the
 * parsed configuration: a generated file would lose the comments the first run wrote, and this
 * project's whole configuration story is one well-commented file.
 *
 * If the key is missing (an older file, or one the operator trimmed), it is inserted right after
 * the `[server]` header rather than appended, which would land it in whatever table happened to
 * come last.
 */
public fun rewriteOperators(
    names: Set<String>,
    path: String = "basalt.toml",
    logger: Logger = Logger("Config"),
) {
    val text = FileIO.readText(path)
    if (text == null) {
        logger.warn { "Could not read $path, so the operator list was not saved." }
        return
    }
    val rendered = "operators = [" + names.sorted().joinToString(", ") { "\"$it\"" } + "]"
    val lines = text.lines().toMutableList()
    val existing = lines.indexOfFirst { it.trimStart().startsWith("operators") && it.contains('=') }
    if (existing >= 0) {
        lines[existing] = rendered
    } else {
        val serverHeader = lines.indexOfFirst { it.trim() == "[server]" }
        if (serverHeader < 0) {
            logger.warn { "$path has no [server] table, so the operator list was not saved." }
            return
        }
        lines.add(serverHeader + 1, rendered)
    }
    FileIO.writeText(path, lines.joinToString("\n"))
}
