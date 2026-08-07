package org.basaltmc.server

import org.basaltmc.assets.EulaFile
import org.basaltmc.auth.MojangSessionService
import org.basaltmc.auth.OnlineMode
import org.basaltmc.auth.SessionKeyPair
import org.basaltmc.common.logging.Logger
import org.basaltmc.server.config.BasaltConfig
import kotlin.system.exitProcess

public fun main(args: Array<String>) {
    println("Basalt ${BuildInfo.VERSION} (protocol ${BuildInfo.PROTOCOL_VERSION})")

    // No readLine()/prompts, no TTY detection: identical behavior interactive, headless, or
    // containerized. --accept-eula is a deliberate operator act, not a fallback.
    val logger = Logger("Eula")
    val eulaResult = EulaFile.check(acceptFlag = "--accept-eula" in args, logger = logger)
    if (eulaResult is EulaFile.Result.Rejected) {
        exitProcess(EulaFile.REJECTED_EXIT_CODE)
    }

    val config = BasaltConfig.load()
    val onlineMode = if (config.onlineMode) OnlineMode(SessionKeyPair.generate(), MojangSessionService()) else null
    BasaltServer(config, onlineMode).start()
}
