package org.basaltmc.assets

import org.basaltmc.common.logging.Logger
import org.basaltmc.io.FileIO
import kotlin.time.Clock
import kotlin.time.ExperimentalTime

/**
 * // parity: net.minecraft.server.Eula, minus the prompt: no TTY detection, identical interactive,
 * headless or in a container. `--accept-eula` is a deliberate operator act and counts as explicit
 * acceptance without touching the file. The default file links minecraft.net rather than vanilla's
 * `aka.ms` redirect, same document.
 *
 * [PATH] is TOML because the EULA asks for agreement, not for a file name, and vanilla's own body
 * already parses as TOML. [LEGACY_PATH] is still honored and migrated once: hosting panels write
 * `eula.txt` themselves, and refusing it would stop those servers over a file they never had.
 */
public object EulaFile {
    public const val PATH: String = "eula.toml"

    /** Vanilla's own name, still read so panels and migrated servers keep working. */
    public const val LEGACY_PATH: String = "eula.txt"

    /** Stable, documented exit code for a rejected/missing EULA: distinguishes it from a crash. */
    public const val REJECTED_EXIT_CODE: Int = 78

    public sealed class Result {
        public data object Accepted : Result()

        /** [justCreated] is true the very first time, when [PATH] didn't exist yet. */
        public data class Rejected(
            val justCreated: Boolean,
        ) : Result()
    }

    public fun check(
        acceptFlag: Boolean,
        logger: Logger,
    ): Result {
        if (acceptFlag) {
            logger.info { "EULA accepted via --accept-eula." }
            return Result.Accepted
        }

        FileIO.readText(PATH)?.let { content ->
            if (isAccepted(content)) return Result.Accepted
            logger.warn { "You must accept the Minecraft EULA (eula = true in $PATH) to run this server." }
            return Result.Rejected(justCreated = false)
        }

        val legacy = FileIO.readText(LEGACY_PATH)
        if (legacy != null && isAccepted(legacy)) {
            FileIO.writeText(PATH, defaultFileContent(accepted = true))
            logger.info { "Found an accepted $LEGACY_PATH, recording it in $PATH. $LEGACY_PATH is no longer read." }
            return Result.Accepted
        }

        FileIO.writeText(PATH, defaultFileContent(accepted = false))
        logger.warn { "$PATH created. You must accept the Minecraft EULA (eula = true) to run this server." }
        return Result.Rejected(justCreated = true)
    }

    /** Accepts vanilla's spacing-free `eula=true` as readily as TOML's `eula = true`. */
    internal fun isAccepted(content: String): Boolean =
        content.lineSequence().any { line ->
            val trimmed = line.trim()
            !trimmed.startsWith("#") && trimmed.replace(" ", "").equals("eula=true", ignoreCase = true)
        }

    @OptIn(ExperimentalTime::class)
    private fun defaultFileContent(accepted: Boolean): String =
        buildString {
            appendLine("# By changing the setting below to true you are indicating your agreement to our EULA")
            appendLine("# https://www.minecraft.net/en-us/eula")
            appendLine("# ${Clock.System.now()}")
            append("eula = $accepted")
        }
}
