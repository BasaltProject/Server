package org.basaltmc.server

public fun main() {
    println("Basalt ${BuildInfo.VERSION} (protocol ${BuildInfo.PROTOCOL_VERSION})")
    BasaltServer(port = 25565).start()
}
