package org.basaltmc.server.player

import org.basaltmc.server.ClientSession
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/** Server-side bookkeeping for one logged-in player: identity, live position, and how to reach their connection. */
@OptIn(ExperimentalUuidApi::class)
public class PlayerSession(
    public val uuid: Uuid,
    public val name: String,
    public val entityId: Int,
    internal val connection: ClientSession,
) {
    public var x: Double = 0.0
    public var y: Double = 0.0
    public var z: Double = 0.0
    public var yaw: Float = 0f
    public var pitch: Float = 0f
    public var onGround: Boolean = false
}
