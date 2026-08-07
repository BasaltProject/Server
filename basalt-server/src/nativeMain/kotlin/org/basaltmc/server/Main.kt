package org.basaltmc.server

import org.basaltmc.auth.MojangSessionService
import org.basaltmc.auth.OnlineMode
import org.basaltmc.auth.SessionKeyPair
import org.basaltmc.server.config.BasaltConfig

public fun main() {
    println("Basalt ${BuildInfo.VERSION} (protocol ${BuildInfo.PROTOCOL_VERSION})")
    val config = BasaltConfig.load()
    val onlineMode = if (config.onlineMode) OnlineMode(SessionKeyPair.generate(), MojangSessionService()) else null
    BasaltServer(config, onlineMode).start()
}
