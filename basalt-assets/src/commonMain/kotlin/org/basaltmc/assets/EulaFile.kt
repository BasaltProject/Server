package org.basaltmc.assets

import org.basaltmc.common.logging.Logger
import org.basaltmc.io.FileIO
import kotlin.time.Clock
import kotlin.time.ExperimentalTime

/**
 * The EULA acceptance mechanism: no prompts, no TTY detection, works identically interactive,
 * headless, or in a container. `--accept-eula` (see [check]'s `acceptFlag`) is a deliberate
 * operator act and counts as explicit acceptance without touching the file.
 *
 * // parity: `net.minecraft.server.Eula`. Real vanilla's default file has a link to
 * `https://aka.ms/MinecraftEULA`; this project links the canonical `minecraft.net` URL instead,
 * same legal document, not a Mojang-owned redirect.
 */
public object EulaFile {
    public const val PATH: String = "eula.txt"

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

        val content = FileIO.readText(PATH)
        if (content == null) {
            FileIO.writeText(PATH, defaultFileContent())
            logger.warn { "$PATH created. You must accept the Minecraft EULA (eula=true) to run this server." }
            return Result.Rejected(justCreated = true)
        }

        if (isAccepted(content)) {
            return Result.Accepted
        }

        logger.warn { "You must accept the Minecraft EULA (eula=true in $PATH) to run this server." }
        return Result.Rejected(justCreated = false)
    }

    internal fun isAccepted(content: String): Boolean =
        content.lineSequence().any { line ->
            val trimmed = line.trim()
            !trimmed.startsWith("#") && trimmed.replace(" ", "").equals("eula=true", ignoreCase = true)
        }

    @OptIn(ExperimentalTime::class)
    private fun defaultFileContent(): String =
        buildString {
            appendLine("# By changing the setting below to TRUE you are indicating your agreement to our EULA")
            appendLine("# https://www.minecraft.net/en-us/eula")
            appendLine("# ${Clock.System.now()}")
            append("eula=false")
        }
}
